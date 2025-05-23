// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "unix/ibus/selection_monitor.h"

#include <xcb/xcb.h>
#include <xcb/xfixes.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/thread2.h"
#include "base/util.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace mozc {
namespace ibus {
namespace {

class ScopedXcbGenericError {
 public:
  ScopedXcbGenericError() : error_(nullptr) {}
  ~ScopedXcbGenericError() {
    free(error_);
    error_ = nullptr;
  }
  const xcb_generic_error_t *get() const { return error_; }
  xcb_generic_error_t **mutable_get() { return &error_; }

 private:
  xcb_generic_error_t *error_;
};

template <typename T>
struct FreeDeleter {
  void operator()(T *ptr) const { free(ptr); }
};

// TODO(yukawa): Use template aliases when GCC 4.6 is retired.
typedef std::unique_ptr<xcb_get_property_reply_t,
                        FreeDeleter<xcb_get_property_reply_t>>
    ScopedXcbGetPropertyReply;
typedef std::unique_ptr<xcb_get_atom_name_reply_t,
                        FreeDeleter<xcb_get_atom_name_reply_t>>
    ScopedXcbGetAtomNameReply;
typedef std::unique_ptr<xcb_intern_atom_reply_t,
                        FreeDeleter<xcb_intern_atom_reply_t>>
    ScopedXcbInternAtomReply;
typedef std::unique_ptr<xcb_xfixes_query_version_reply_t,
                        FreeDeleter<xcb_xfixes_query_version_reply_t>>
    ScopedXcbXFixesQueqyVersionReply;

struct XcbAtoms {
  xcb_atom_t mozc_selection_monitor;
  xcb_atom_t net_wm_name;
  xcb_atom_t net_wm_pid;
  xcb_atom_t utf8_string;
  xcb_atom_t wm_client_machine;
  XcbAtoms()
      : mozc_selection_monitor(XCB_NONE),
        net_wm_name(XCB_NONE),
        net_wm_pid(XCB_NONE),
        utf8_string(XCB_NONE),
        wm_client_machine(XCB_NONE) {}
};

class SelectionMonitorServer {
 public:
  SelectionMonitorServer()
      : connection_(nullptr),
        requestor_window_(0),
        root_window_(0),
        xfixes_first_event_(0),
        xcb_maximum_request_len_(0) {}

  SelectionMonitorServer(const SelectionMonitorServer &) = delete;
  SelectionMonitorServer &operator=(const SelectionMonitorServer &) = delete;

  ~SelectionMonitorServer() { Release(); }

  bool Init() {
    connection_ = ::xcb_connect(nullptr, nullptr);
    if (connection_ == nullptr) {
      return false;
    }

    if (!InitXFixes()) {
      Release();
      return false;
    }

    if (!InitAtoms()) {
      Release();
      return false;
    }

    const xcb_screen_t *screen =
        ::xcb_setup_roots_iterator(::xcb_get_setup(connection_)).data;

    requestor_window_ = ::xcb_generate_id(connection_);
    const uint32_t mask = XCB_CW_EVENT_MASK;
    const uint32_t values[] = {XCB_EVENT_MASK_PROPERTY_CHANGE};
    root_window_ = screen->root;
    ::xcb_create_window(connection_, screen->root_depth, requestor_window_,
                        root_window_, 0, 0, 1, 1, 0,
                        XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                        mask, values);
    const uint32_t xfixes_mask =
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
        XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
        XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;
    ::xcb_xfixes_select_selection_input_checked(connection_, requestor_window_,
                                                XCB_ATOM_PRIMARY, xfixes_mask);
    ::xcb_flush(connection_);
    return true;
  }

  bool checkConnection() {
    if (!connection_) {
      return false;
    }
    if (::xcb_connection_has_error(connection_)) {
      LOG(ERROR) << "XCB connection has error.";
      connection_ = nullptr;
      return false;
    }
    return true;
  }

  bool WaitForNextSelectionEvent(size_t max_bytes, SelectionInfo *next_info) {
    DCHECK(next_info);
    if (!connection_) {
      return false;
    }

    ::xcb_flush(connection_);
    std::unique_ptr<xcb_generic_event_t, void (*)(void *)> event(
        ::xcb_wait_for_event(connection_), &std::free);

    if (event.get() == nullptr) {
      LOG(ERROR) << "nullptr event returned.";
      return false;
    }

    const uint32_t response_type = (event->response_type & ~0x80);

    if (response_type == (xfixes_first_event_ + XCB_XFIXES_SELECTION_NOTIFY)) {
      return OnXFixesSelectionNotify(event.get(), max_bytes, next_info);
    }

    if (response_type != XCB_SELECTION_NOTIFY) {
      VLOG(2) << "Ignored a message. response_type: " << response_type;
      return false;
    }

    return OnSelectionNotify(event.get(), max_bytes, next_info);
  }

  // Sends a harmless message to the |requestor_window_|. You can call this
  // method to awake a message pump thread which is waiting for the next
  // X11 message for |requestor_window_|.
  void SendNoopEventMessage() {
    if (!connection_ || !requestor_window_) {
      return;
    }

    // Send a dummy event so that the event pump can wake up.
    xcb_client_message_event_t event = {};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = requestor_window_;
    event.format = 32;
    event.type = XCB_NONE;
    ::xcb_send_event(connection_, false, requestor_window_,
                     XCB_EVENT_MASK_NO_EVENT,
                     reinterpret_cast<const char *>(&event));
    ::xcb_flush(connection_);
  }

 private:
  void Release() {
    if (connection_) {
      ::xcb_disconnect(connection_);
      connection_ = nullptr;
    }
  }

  bool CreateAtom(const std::string &name, xcb_atom_t *atom) const {
    DCHECK(atom);
    *atom = XCB_NONE;
    xcb_intern_atom_cookie_t cookie =
        ::xcb_intern_atom(connection_, false, name.size(), name.c_str());
    ScopedXcbInternAtomReply reply(
        ::xcb_intern_atom_reply(connection_, cookie, 0));
    if (reply.get() == nullptr) {
      LOG(ERROR) << "xcb_intern_atom_reply returned nullptr reply.";
      return false;
    }
    if (reply->atom == XCB_NONE) {
      return false;
    }
    *atom = reply->atom;
    return true;
  }

  bool InitAtoms() {
    return CreateAtom("MOZC_SEL_MON", &atoms_.mozc_selection_monitor) &&
           CreateAtom("UTF8_STRING", &atoms_.utf8_string) &&
           CreateAtom("_NET_WM_NAME", &atoms_.net_wm_name) &&
           CreateAtom("_NET_WM_PID", &atoms_.net_wm_pid) &&
           CreateAtom("WM_CLIENT_MACHINE", &atoms_.wm_client_machine);
  }

  bool InitXFixes() {
    const xcb_query_extension_reply_t *ext_reply =
        ::xcb_get_extension_data(connection_, &xcb_xfixes_id);
    if (ext_reply == nullptr) {
      LOG(ERROR) << "xcb_get_extension_data returns nullptr.";
      return false;
    }

    const xcb_xfixes_query_version_cookie_t xfixes_query_cookie =
        xcb_xfixes_query_version(connection_, XCB_XFIXES_MAJOR_VERSION,
                                 XCB_XFIXES_MINOR_VERSION);
    ScopedXcbGenericError xcb_error;
    ScopedXcbXFixesQueqyVersionReply xfixes_query(
        ::xcb_xfixes_query_version_reply(connection_, xfixes_query_cookie,
                                         xcb_error.mutable_get()));
    if (xcb_error.get() != nullptr) {
      LOG(ERROR) << "xcb_xfixes_query_version_reply failed. error_code: "
                 << static_cast<uint32_t>(xcb_error.get()->error_code);
      return false;
    }
    if (xfixes_query.get() == nullptr) {
      return false;
    }

    xfixes_first_event_ = ext_reply->first_event;
    LOG(INFO) << "XFixes ver: " << xfixes_query->major_version << "."
              << xfixes_query->major_version
              << ", first_event: " << xfixes_first_event_;

    xcb_maximum_request_len_ = ::xcb_get_maximum_request_length(connection_);
    if (xcb_maximum_request_len_ <= 0) {
      LOG(ERROR) << "Unexpected xcb maximum request length: "
                 << xcb_maximum_request_len_;
      return false;
    }

    return true;
  }

  std::string GetAtomName(xcb_atom_t atom) const {
    const xcb_get_atom_name_cookie_t cookie =
        ::xcb_get_atom_name(connection_, atom);
    ScopedXcbGenericError xcb_error;
    ScopedXcbGetAtomNameReply reply(::xcb_get_atom_name_reply(
        connection_, cookie, xcb_error.mutable_get()));
    if (xcb_error.get() != nullptr) {
      LOG(ERROR) << "xcb_get_atom_name_reply failed. error_code: "
                 << static_cast<uint32_t>(xcb_error.get()->error_code);
      return "";
    }
    if (reply.get() == nullptr) {
      VLOG(2) << "reply is nullptr";
      return "";
    }

    const char *ptr = ::xcb_get_atom_name_name(reply.get());
    const size_t len = ::xcb_get_atom_name_name_length(reply.get());
    return std::string(ptr, len);
  }

  bool GetByteArrayProperty(xcb_window_t window, xcb_atom_t property_atom,
                            xcb_atom_t property_type_atom, size_t max_bytes,
                            std::string *retval) const {
    DCHECK(retval);
    retval->clear();
    size_t bytes_after = 0;
    int element_bit_size = 0;
    {
      const xcb_get_property_cookie_t cookie = ::xcb_get_property(
          connection_, false, window, property_atom, property_type_atom, 0, 0);
      ScopedXcbGetPropertyReply reply(
          ::xcb_get_property_reply(connection_, cookie, 0));
      if (reply.get() == nullptr) {
        VLOG(2) << "reply is nullptr";
        return false;
      }
      if (reply->type == XCB_NONE) {
        LOG(ERROR) << "reply type is XCB_NONE";
        return false;
      }
      if (reply->type != property_type_atom) {
        LOG(ERROR) << "unexpected atom type: " << GetAtomName(reply->type);
        return false;
      }
      bytes_after = reply->bytes_after;
      element_bit_size = reply->format;
    }

    if (max_bytes < bytes_after) {
      LOG(WARNING) << "Exceeds size limit. Returns an empty string."
                   << " max_bytes: " << max_bytes
                   << ", bytes_after: " << bytes_after;
      *retval = "";
      return true;
    }

    if (element_bit_size == 0) {
      VLOG(1) << "element_bit_size is 0. Assuming byte-size data.";
      element_bit_size = 8;
    }

    if (element_bit_size != 8) {
      LOG(ERROR) << "Unsupported bit size: " << element_bit_size;
      return false;
    }

    int byte_offset = 0;

    while (bytes_after > 0) {
      const xcb_get_property_cookie_t cookie =
          ::xcb_get_property(connection_, false, window, property_atom,
                             property_type_atom, byte_offset, max_bytes);
      ScopedXcbGetPropertyReply reply(
          ::xcb_get_property_reply(connection_, cookie, 0));
      if (reply.get() == nullptr) {
        VLOG(2) << "reply is nullptr";
        return false;
      }
      if (reply->format != element_bit_size) {
        LOG(ERROR) << "bit size changed: " << reply->format;
        return false;
      }
      bytes_after = reply->bytes_after;
      const char *data =
          reinterpret_cast<const char *>(::xcb_get_property_value(reply.get()));
      const int length = ::xcb_get_property_value_length(reply.get());
      *retval += std::string(data, length);
      byte_offset += length;
    }
    return true;
  }

  template <typename T>
  bool GetCardinalProperty(xcb_window_t window, xcb_atom_t property_atom,
                           T *retval) const {
    *retval = 0;
    const xcb_get_property_cookie_t cookie =
        ::xcb_get_property(connection_, false, window, property_atom,
                           XCB_ATOM_CARDINAL, 0, sizeof(T) * 8);
    ScopedXcbGetPropertyReply reply(
        ::xcb_get_property_reply(connection_, cookie, 0));
    if (reply.get() == nullptr) {
      VLOG(2) << "reply is nullptr";
      return false;
    }

    if (reply->type != XCB_ATOM_CARDINAL) {
      LOG(ERROR) << "unexpected type: " << GetAtomName(reply->type);
      return false;
    }

    // All data should be read.
    if (reply->bytes_after != 0) {
      LOG(ERROR) << "unexpectedly " << reply->bytes_after
                 << " bytes data remain.";
      return false;
    }
    if (reply->format != 0 && (reply->format != sizeof(T) * 8)) {
      LOG(ERROR) << "unexpected bit size: " << reply->format;
      return false;
    }

    *retval =
        *reinterpret_cast<const T *>(::xcb_get_property_value(reply.get()));
    return true;
  }

  bool OnXFixesSelectionNotify(const xcb_generic_event_t *event,
                               size_t max_bytes, SelectionInfo *next_info) {
    const xcb_xfixes_selection_notify_event_t *event_notify =
        reinterpret_cast<const xcb_xfixes_selection_notify_event_t *>(event);
    if (event_notify->selection != XCB_ATOM_PRIMARY) {
      VLOG(2) << "Ignored :" << GetAtomName(event_notify->selection);
      return false;
    }

    // Send request message for selection info.
    ::xcb_convert_selection(connection_, requestor_window_, XCB_ATOM_PRIMARY,
                            atoms_.utf8_string, atoms_.mozc_selection_monitor,
                            XCB_CURRENT_TIME);

    last_request_info_.timestamp = event_notify->selection_timestamp;

    uint32_t net_wm_pid = 0;
    if (GetCardinalProperty(event_notify->owner, atoms_.net_wm_pid,
                            &net_wm_pid)) {
      last_request_info_.process_id = net_wm_pid;
    }

    std::string net_wm_name;
    if (GetByteArrayProperty(event_notify->owner, atoms_.net_wm_name,
                             atoms_.utf8_string, max_bytes, &net_wm_name)) {
      last_request_info_.window_title = net_wm_name;
    }

    std::string wm_client_machine;
    if (GetByteArrayProperty(event_notify->owner, atoms_.wm_client_machine,
                             XCB_ATOM_STRING, max_bytes, &wm_client_machine)) {
      last_request_info_.machine_name = wm_client_machine;
    }

    *next_info = last_request_info_;
    return true;
  }

  bool OnSelectionNotify(const xcb_generic_event_t *event, size_t max_bytes,
                         SelectionInfo *next_info) {
    const xcb_selection_notify_event_t *event_notify =
        reinterpret_cast<const xcb_selection_notify_event_t *>(event);
    if (event_notify->selection != XCB_ATOM_PRIMARY) {
      VLOG(2) << "Ignored a message. selection type:"
              << event_notify->selection;
      return false;
    }

    if (event_notify->property == XCB_NONE) {
      VLOG(2) << "Ignored a message whose property type is XCB_NONE";
      return false;
    }

    std::string selected_text;
    if (!GetByteArrayProperty(event_notify->requestor, event_notify->property,
                              atoms_.utf8_string, max_bytes, &selected_text)) {
      LOG(ERROR) << "Failed to retrieve selection text.";
      return false;
    }

    // Update the result.
    *next_info = last_request_info_;
    next_info->selected_text = selected_text;
    return true;
  }

  xcb_connection_t *connection_;
  xcb_window_t requestor_window_;
  xcb_window_t root_window_;
  uint32_t xfixes_first_event_;
  uint32_t xcb_maximum_request_len_;
  SelectionInfo last_request_info_;
  XcbAtoms atoms_;
};

class SelectionMonitorImpl : public SelectionMonitorInterface {
 public:
  SelectionMonitorImpl(SelectionMonitorServer *server, size_t max_text_bytes)
      : server_(server), max_text_bytes_(max_text_bytes) {}

  SelectionMonitorImpl(const SelectionMonitorImpl &) = delete;
  SelectionMonitorImpl &operator=(const SelectionMonitorImpl &) = delete;

  ~SelectionMonitorImpl() override {
    if (thread_.has_value()) {
      QueryQuit();
      thread_->Join();
    }
  }

  // Implements SelectionMonitorInterface::StartMonitoring.
  void StartMonitoring() override {
    if (thread_.has_value()) {
      QueryQuit();
      thread_->Join();
    }
    thread_ = mozc::Thread2([this] {
      while (!quit_.HasBeenNotified()) {
        if (!server_->checkConnection()) {
          absl::MutexLock l(&mutex_);
          last_selection_info_ = SelectionInfo();
          quit_.Notify();
          break;
        }

        SelectionInfo next_info;
        // Note that this is a blocking call and will not return until the next
        // X11 message is received. In order to interrupt, you can call
        // SendNoopEventMessage() from other threads.
        if (server_->WaitForNextSelectionEvent(max_text_bytes_, &next_info)) {
          absl::MutexLock l(&mutex_);
          last_selection_info_ = next_info;
        }
      }
    });
  }

  // Implements SelectionMonitorInterface::QueryQuit.
  void QueryQuit() override {
    if (!quit_.HasBeenNotified()) {
      quit_.Notify();
      // Awake the message pump thread so that it can see the updated
      // |quit_| immediately.
      server_->SendNoopEventMessage();
    }
  }

  // Implements SelectionMonitorInterface::GetSelectionInfo.
  SelectionInfo GetSelectionInfo() override {
    SelectionInfo info;
    {
      absl::MutexLock l(&mutex_);
      info = last_selection_info_;
    }
    return info;
  }

 private:
  absl::Notification quit_;
  std::optional<mozc::Thread2> thread_;
  std::unique_ptr<SelectionMonitorServer> server_;
  const size_t max_text_bytes_;
  absl::Mutex mutex_;
  SelectionInfo last_selection_info_;
};

}  // namespace

SelectionMonitorInterface::~SelectionMonitorInterface() {}

SelectionInfo::SelectionInfo() : timestamp(0), process_id(0) {}

SelectionMonitorInterface *SelectionMonitorFactory::Create(
    size_t max_text_bytes) {
  std::unique_ptr<SelectionMonitorServer> server(new SelectionMonitorServer());
  if (!server->Init()) {
    return nullptr;
  }
  return new SelectionMonitorImpl(server.release(), max_text_bytes);
}

}  // namespace ibus
}  // namespace mozc
