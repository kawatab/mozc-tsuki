// Copyright 2010-2014, Google Inc.
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

// Mock of IPCClientFactoryInterface and IPCClientInterface for unittesting.

#include "ipc/ipc_mock.h"

#include "base/version.h"
#include "ipc/ipc.h"

#include <cstring>  // memcpy

namespace mozc {

IPCClientMock::IPCClientMock(IPCClientFactoryMock *caller)
      : caller_(caller),
        connected_(false),
        server_protocol_version_(0),
        server_product_version_(Version::GetMozcVersion()),
        server_process_id_(0),
        result_(false) {
}

bool IPCClientMock::Connected() const {
  return connected_;
}

uint32 IPCClientMock::GetServerProtocolVersion() const {
  return server_protocol_version_;
}

const string &IPCClientMock::GetServerProductVersion() const {
  return server_product_version_;
}


uint32 IPCClientMock::GetServerProcessId() const {
  return server_process_id_;
}

bool IPCClientMock::Call(const char *request,
                         const size_t request_size,
                         char *response,
                         size_t *response_size,
                         const int32 timeout) {
  caller_->SetGeneratedRequest(string(request, request_size));
  if (!connected_ || !result_) {
    return false;
  }
  memcpy(response, response_.c_str(), response_.length());
  *response_size = response_.length();
  return true;
}

IPCClientFactoryMock::IPCClientFactoryMock()
    : connection_(false), result_(false),
      server_protocol_version_(IPC_PROTOCOL_VERSION) {
}

IPCClientInterface *IPCClientFactoryMock::NewClient(const string &unused_name,
                                                    const string &path_name) {
  return NewClientMock();
}

IPCClientInterface *IPCClientFactoryMock::NewClient(const string &unused_name) {
  return NewClientMock();
}

const string &IPCClientFactoryMock::GetGeneratedRequest() const {
  return request_;
}

void IPCClientFactoryMock::SetGeneratedRequest(const string &request) {
  request_ = request;
}

void IPCClientFactoryMock::SetMockResponse(const string &response) {
  response_ = response;
}

void IPCClientFactoryMock::SetConnection(const bool connection) {
  connection_ = connection;
}

void IPCClientFactoryMock::SetResult(const bool result) {
  result_ = result;
}

void IPCClientFactoryMock::SetServerProtocolVersion(const uint32
                                                    server_protocol_version) {
  server_protocol_version_ = server_protocol_version;
}

void IPCClientFactoryMock::SetServerProductVersion(const string &
                                                   server_product_version) {
  server_product_version_ = server_product_version;
}


void IPCClientFactoryMock::SetServerProcessId(const uint32
                                              server_process_id) {
  server_process_id_ = server_process_id;
}

IPCClientMock *IPCClientFactoryMock::NewClientMock() {
  IPCClientMock *client = new IPCClientMock(this);
  client->set_connection(connection_);
  client->set_result(result_);
  client->set_response(response_);
  client->set_server_protocol_version(server_protocol_version_);
  client->set_server_product_version(server_product_version_);
  return client;
}

}  // namespace mozc
