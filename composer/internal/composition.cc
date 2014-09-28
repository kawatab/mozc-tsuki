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

#include "composer/internal/composition.h"

#include "base/logging.h"
#include "base/util.h"
#include "composer/internal/char_chunk.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"

namespace mozc {
namespace composer {

Composition::Composition(const Table *table)
    : table_(table), input_t12r_(Transliterators::CONVERSION_STRING) {}

Composition::~Composition() {
  Erase();
}

void Composition::Erase() {
  CharChunkList::iterator it;
  for (it = chunks_.begin(); it != chunks_.end(); ++it) {
    delete *it;
  }
  chunks_.clear();
}

size_t Composition::InsertAt(size_t pos, const string &input) {
  CompositionInput composition_input;
  composition_input.set_raw(input);
  return InsertInput(pos, composition_input);
}

size_t Composition::InsertKeyAndPreeditAt(const size_t pos,
                                          const string &key,
                                          const string &preedit) {
  CompositionInput composition_input;
  composition_input.set_raw(key);
  composition_input.set_conversion(preedit);
  return InsertInput(pos, composition_input);
}

size_t Composition::InsertInput(size_t pos, const CompositionInput &input) {
  if (input.Empty()) {
    return pos;
  }

  CharChunkList::iterator right_chunk;
  MaybeSplitChunkAt(pos, &right_chunk);

  CharChunkList::iterator left_chunk = GetInsertionChunk(&right_chunk);
  CombinePendingChunks(left_chunk, input);

  CompositionInput mutable_input;
  mutable_input.CopyFrom(input);
  while (true) {
    (*left_chunk)->AddCompositionInput(&mutable_input);
    if (mutable_input.Empty()) {
      break;
    }
    left_chunk = InsertChunk(&right_chunk);
    mutable_input.set_is_new_input(false);
  }

  return GetPosition(Transliterators::LOCAL, right_chunk);
}

// Deletes a right-hand character of the composition.
size_t Composition::DeleteAt(const size_t position) {
  CharChunkList::iterator chunk_it;
  const size_t original_size = GetLength();
  size_t new_position = position;
  // We have to perform deletion repeatedly because there might be 0-length
  // chunk.
  // For example,
  // chunk0 : '{a}'  (invisible character only == 0-length)
  // chunk1 : 'b'
  // And DeleteAt(0) is invoked, we have to delete both chunks.
  while (!chunks_.empty() && GetLength() == original_size) {
    MaybeSplitChunkAt(position, &chunk_it);
    new_position = GetPosition(Transliterators::LOCAL, chunk_it);
    if (chunk_it == chunks_.end()) {
      break;
    }

    // We have to consider 0-length chunk.
    // If a chunk contains only invisible characters,
    // the result of GetLength is 0.
    if ((*chunk_it)->GetLength(Transliterators::LOCAL) <= 1) {
      delete *chunk_it;
      chunks_.erase(chunk_it);
      continue;
    }

    CharChunk *left_deleted_chunk_ptr = NULL;
    (*chunk_it)->SplitChunk(Transliterators::LOCAL, 1, &left_deleted_chunk_ptr);
    scoped_ptr<CharChunk> left_deleted_chunk(left_deleted_chunk_ptr);
  }
  return new_position;
}

size_t Composition::ConvertPosition(
    const size_t position_from,
    Transliterators::Transliterator transliterator_from,
    Transliterators::Transliterator transliterator_to) {
  // TODO(komatsu) This is a hacky way.
  if (transliterator_from == transliterator_to) {
    return position_from;
  }

  CharChunkList::iterator chunk_it;
  size_t inner_position_from;
  GetChunkAt(position_from, transliterator_from,
             &chunk_it, &inner_position_from);

  // No chunk was found, return 0 as a fallback.
  if (chunk_it == chunks_.end()) {
    return 0;
  }

  const size_t chunk_length_from = (*chunk_it)->GetLength(transliterator_from);

  CHECK(inner_position_from <= chunk_length_from);

  const size_t position_to = GetPosition(transliterator_to, chunk_it);

  if (inner_position_from == 0) {
    return position_to;
  }

  const size_t chunk_length_to = (*chunk_it)->GetLength(transliterator_to);
  if (inner_position_from == chunk_length_from) {
    // If the inner_position_from is the end of the chunk (ex. "ka|"
    // vs "か"), the converterd position should be the end of the
    // chunk too (ie. "か|").
    return position_to + chunk_length_to;
  }


  if (inner_position_from > chunk_length_to) {
    // When inner_position_from is greater than chunk_length_to
    // (ex. "ts|u" vs "つ", inner_position_from is 2 and
    // chunk_length_to is 1), the converted position should be the end
    // of the chunk (ie. "つ|").
    return position_to + chunk_length_to;
  }

  DCHECK(inner_position_from <= chunk_length_to);
  // When inner_position_from is less than or equal to chunk_length_to
  // (ex. "っ|と" vs "tto", inner_position_from is 1 and
  // chunk_length_to is 2), the converted position is adjusted from
  // the beginning of the chunk (ie. "t|to").
  return position_to + inner_position_from;
}

size_t Composition::SetDisplayMode(
    const size_t position,
    Transliterators::Transliterator transliterator) {
  SetTransliterator(0, GetLength(), transliterator);
  SetInputMode(transliterator);
  return GetLength();
}

void Composition::SetTransliterator(
    const size_t position_from,
    const size_t position_to,
    Transliterators::Transliterator transliterator) {
  if (position_from > position_to) {
    LOG(ERROR) << "position_from should not be greater than position_to.";
    return;
  }

  if (chunks_.empty()) {
    return;
  }

  CharChunkList::iterator chunk_it;
  size_t inner_position_from;
  GetChunkAt(position_from, Transliterators::LOCAL, &chunk_it,
             &inner_position_from);

  CharChunkList::iterator end_it;
  size_t inner_position_to;
  GetChunkAt(position_to, Transliterators::LOCAL, &end_it, &inner_position_to);

  // chunk_it and end_it can be the same iterator from the beginning.
  while (chunk_it != end_it) {
    (*chunk_it)->SetTransliterator(transliterator);
    ++chunk_it;
  }
  (*end_it)->SetTransliterator(transliterator);
}

Transliterators::Transliterator
Composition::GetTransliterator(size_t position) {
  // Due to GetChunkAt is not a const funcion, this function cannot be
  // a const function.
  CharChunkList::iterator chunk_it;
  size_t inner_position;
  GetChunkAt(position, Transliterators::LOCAL, &chunk_it, &inner_position);
  return (*chunk_it)->GetTransliterator(Transliterators::LOCAL);
}

size_t Composition::GetLength() const {
  return GetPosition(Transliterators::LOCAL, chunks_.end());
}

void Composition::GetStringWithModes(
    Transliterators::Transliterator transliterator,
    const TrimMode trim_mode,
    string* composition) const {
  composition->clear();
  if (chunks_.empty()) {
    // This is not an error. For example, the composition should be empty for
    // the first keydown event after turning on the IME.
    DCHECK(composition->empty()) << "An empty string should be returned.";
    return;
  }

  CharChunkList::const_iterator it;
  for (it = chunks_.begin(); *it != chunks_.back(); ++it) {
    (*it)->AppendResult(transliterator, composition);
  }

  switch (trim_mode) {
    case TRIM:
      (*it)->AppendTrimedResult(transliterator, composition);
      break;
    case ASIS:
      (*it)->AppendResult(transliterator, composition);
      break;
    case FIX:
      (*it)->AppendFixedResult(transliterator, composition);
      break;
    default:
      LOG(WARNING) << "Unexpected trim mode: " << trim_mode;
      break;
  }
}

void Composition::GetExpandedStrings(string *base,
                                     set<string> *expanded) const {
  GetExpandedStringsWithTransliterator(Transliterators::LOCAL, base, expanded);
}

void Composition::GetExpandedStringsWithTransliterator(
    Transliterators::Transliterator transliterator,
    string *base,
    set<string> *expanded) const {
  DCHECK(base);
  DCHECK(expanded);
  base->clear();
  expanded->clear();
  if (chunks_.empty()) {
    VLOG(1) << "The composition size is zero.";
    return;
  }

  CharChunkList::const_iterator it;
  for (it = chunks_.begin(); (*it) != chunks_.back(); ++it) {
    (*it)->AppendFixedResult(transliterator, base);
  }

  chunks_.back()->AppendTrimedResult(transliterator, base);
  // Get expanded from the last chunk
  chunks_.back()->GetExpandedResults(expanded);
}

void Composition::GetString(string *composition) const {
  composition->clear();
  if (chunks_.empty()) {
    VLOG(1) << "The composition size is zero.";
    return;
  }

  for (CharChunkList::const_iterator it = chunks_.begin();
       it != chunks_.end();
       ++it) {
    (*it)->AppendResult(Transliterators::LOCAL, composition);
  }
}

void Composition::GetStringWithTransliterator(
    Transliterators::Transliterator transliterator,
    string* output) const {
  GetStringWithModes(transliterator, FIX, output);
}

void Composition::GetStringWithTrimMode(const TrimMode trim_mode,
                                        string* output) const {
  GetStringWithModes(Transliterators::LOCAL, trim_mode, output);
}

void Composition::GetPreedit(
    size_t position, string *left, string *focused, string *right) const {
  string composition;
  GetString(&composition);
  left->assign(Util::SubString(composition, 0, position));
  focused->assign(Util::SubString(composition, position, 1));
  right->assign(Util::SubString(composition, position + 1, string::npos));
}

// This function is essentialy a const function, but chunks_.begin()
// violates the constness of this function.
void Composition::GetChunkAt(const size_t position,
                             Transliterators::Transliterator transliterator,
                             CharChunkList::iterator *chunk_it,
                             size_t *inner_position) {
  if (chunks_.empty()) {
    *inner_position = 0;
    *chunk_it = chunks_.begin();
    return;
  }

  size_t rest_pos = position;
  CharChunkList::iterator it;
  for (it = chunks_.begin(); it != chunks_.end(); ++it) {
    const size_t chunk_length = (*it)->GetLength(transliterator);
    if (rest_pos <= chunk_length) {
      *inner_position = rest_pos;
      *chunk_it = it;
      return;
    }
    rest_pos -= chunk_length;
  }
  *chunk_it = chunks_.end();
  --(*chunk_it);
  *inner_position = (**chunk_it)->GetLength(transliterator);
}

size_t Composition::GetPosition(
    Transliterators::Transliterator transliterator,
    const CharChunkList::const_iterator &cur_it) const {
  size_t position = 0;
  CharChunkList::const_iterator it;
  for (it = chunks_.begin(); it != cur_it; ++it) {
    position += (*it)->GetLength(transliterator);
  }
  return position;
}

// Return the left CharChunk and the right it.
CharChunk *Composition::MaybeSplitChunkAt(const size_t pos,
                                          CharChunkList::iterator *it) {
  // The position is the beginning of composition.
  if (pos <= 0) {
    *it = chunks_.begin();
    return NULL;
  }

  size_t inner_position;
  GetChunkAt(pos, Transliterators::LOCAL, it, &inner_position);

  CharChunk *chunk = **it;
  if (inner_position == chunk->GetLength(Transliterators::LOCAL)) {
    ++(*it);
    return chunk;
  }

  CharChunk *left_chunk = NULL;
  chunk->SplitChunk(Transliterators::LOCAL, inner_position, &left_chunk);
  chunks_.insert(*it, left_chunk);
  return left_chunk;
}

void Composition::CombinePendingChunks(
    CharChunkList::iterator it, const CompositionInput &input) {
  // Combine |**it| and |**(--it)| into |**it| as long as possible.
  const string &next_input =
    input.has_conversion() ? input.conversion() : input.raw();

  while (it != chunks_.begin()) {
    CharChunkList::iterator left_it = it;
    --left_it;
    if (!(*left_it)->IsConvertible(
            input_t12r_, table_, (*it)->pending() + next_input)) {
      return;
    }

    (*it)->Combine(**left_it);
    delete *left_it;
    chunks_.erase(left_it);
  }
}

// Insert a chunk to the prev of it.
CharChunkList::iterator Composition::InsertChunk(CharChunkList::iterator *it) {
  CharChunk *new_chunk = new CharChunk(input_t12r_, table_);
  return chunks_.insert(*it, new_chunk);
}

const CharChunkList &Composition::GetCharChunkList() const {
  return chunks_;
}

bool Composition::ShouldCommit() const {
  for (CharChunkList::const_iterator it = chunks_.begin();
       it != chunks_.end();
       ++it) {
    if (!(*it)->ShouldCommit()) {
      return false;
    }
  }
  return true;
}

CompositionInterface *Composition::Clone() const {
  return CloneImpl();
}

Composition *Composition::CloneImpl() const {
  Composition *object = new Composition(table_);

  // TODO(hsumita): Implements TableFactory and TransliteratorFactory and uses
  // it instead of copying pointers.
  object->input_t12r_ = input_t12r_;
  object->table_ = table_;

  for (CharChunkList::const_iterator it = chunks_.begin();
       it != chunks_.end(); ++it) {
    object->chunks_.push_back((*it)->Clone());
  }

  return object;
}

// Return charchunk to be inserted and iterator of the *next* char chunk.
CharChunkList::iterator Composition::GetInsertionChunk(
    CharChunkList::iterator *it) {
  if (*it == chunks_.begin()) {
    return InsertChunk(it);
  }

  CharChunkList::iterator left_it = *it;
  --left_it;
  if ((*left_it)->IsAppendable(input_t12r_, table_)) {
    return left_it;
  }
  return InsertChunk(it);
}

void Composition::SetInputMode(Transliterators::Transliterator transliterator) {
  input_t12r_ = transliterator;
}

void Composition::SetTable(const Table *table) {
  table_ = table;
}

}  // namespace composer
}  // namespace mozc
