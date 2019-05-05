#ifndef MOZC_DICTIONARY_POS_MATCHER_H_
#define MOZC_DICTIONARY_POS_MATCHER_H_
#include "./base/port.h"
namespace mozc {
namespace dictionary {
class POSMatcher {
 public:
  // Functional "^(助詞|助動詞|動詞,非自立|名詞,非自立|形容詞,非自立|動詞,接尾|名詞,接尾|形容詞,接尾)"
  inline uint16 GetFunctionalId() const {
    return data_[0];
  }
  inline bool IsFunctional(uint16 id) const {
    const uint16 offset = data_[35 + 0];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // Unknown "名詞,サ変接続"
  inline uint16 GetUnknownId() const {
    return data_[1];
  }
  inline bool IsUnknown(uint16 id) const {
    const uint16 offset = data_[35 + 1];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // FirstName "名詞,固有名詞,人名,名"
  inline uint16 GetFirstNameId() const {
    return data_[2];
  }
  inline bool IsFirstName(uint16 id) const {
    const uint16 offset = data_[35 + 2];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // LastName "名詞,固有名詞,人名,姓"
  inline uint16 GetLastNameId() const {
    return data_[3];
  }
  inline bool IsLastName(uint16 id) const {
    const uint16 offset = data_[35 + 3];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // Number "名詞,数,アラビア数字"
  inline uint16 GetNumberId() const {
    return data_[4];
  }
  inline bool IsNumber(uint16 id) const {
    const uint16 offset = data_[35 + 4];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // KanjiNumber "名詞,数,漢数字"
  inline uint16 GetKanjiNumberId() const {
    return data_[5];
  }
  inline bool IsKanjiNumber(uint16 id) const {
    const uint16 offset = data_[35 + 5];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // WeakCompoundNounPrefix "接頭詞,名詞接続,"
  inline uint16 GetWeakCompoundNounPrefixId() const {
    return data_[6];
  }
  inline bool IsWeakCompoundNounPrefix(uint16 id) const {
    const uint16 offset = data_[35 + 6];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // WeakCompoundVerbPrefix "接頭詞,動詞接続,"
  inline uint16 GetWeakCompoundVerbPrefixId() const {
    return data_[7];
  }
  inline bool IsWeakCompoundVerbPrefix(uint16 id) const {
    const uint16 offset = data_[35 + 7];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // WeakCompoundFillerPrefix "フィラー,"
  inline uint16 GetWeakCompoundFillerPrefixId() const {
    return data_[8];
  }
  inline bool IsWeakCompoundFillerPrefix(uint16 id) const {
    const uint16 offset = data_[35 + 8];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // WeakCompoundNounSuffix "^名詞,(サ変接続|ナイ形容詞語幹|一般|副詞可能|形容詞語幹)"
  inline uint16 GetWeakCompoundNounSuffixId() const {
    return data_[9];
  }
  inline bool IsWeakCompoundNounSuffix(uint16 id) const {
    const uint16 offset = data_[35 + 9];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // WeakCompoundVerbSuffix "動詞,自立"
  inline uint16 GetWeakCompoundVerbSuffixId() const {
    return data_[10];
  }
  inline bool IsWeakCompoundVerbSuffix(uint16 id) const {
    const uint16 offset = data_[35 + 10];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // AcceptableParticleAtBeginOfSegment "^助詞,*,*,*,*,*,(が|で|と|に|にて|の|へ|より|も|と|から|は|や)$"
  inline uint16 GetAcceptableParticleAtBeginOfSegmentId() const {
    return data_[11];
  }
  inline bool IsAcceptableParticleAtBeginOfSegment(uint16 id) const {
    const uint16 offset = data_[35 + 11];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // JapanesePunctuations "記号,(句点|読点)"
  inline uint16 GetJapanesePunctuationsId() const {
    return data_[12];
  }
  inline bool IsJapanesePunctuations(uint16 id) const {
    const uint16 offset = data_[35 + 12];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // OpenBracket "記号,括弧開"
  inline uint16 GetOpenBracketId() const {
    return data_[13];
  }
  inline bool IsOpenBracket(uint16 id) const {
    const uint16 offset = data_[35 + 13];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // CloseBracket "記号,括弧閉"
  inline uint16 GetCloseBracketId() const {
    return data_[14];
  }
  inline bool IsCloseBracket(uint16 id) const {
    const uint16 offset = data_[35 + 14];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // GeneralSymbol "記号,一般,"
  inline uint16 GetGeneralSymbolId() const {
    return data_[15];
  }
  inline bool IsGeneralSymbol(uint16 id) const {
    const uint16 offset = data_[35 + 15];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // Zipcode "特殊,郵便番号"
  inline uint16 GetZipcodeId() const {
    return data_[16];
  }
  inline bool IsZipcode(uint16 id) const {
    const uint16 offset = data_[35 + 16];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // IsolatedWord "特殊,短縮よみ"
  inline uint16 GetIsolatedWordId() const {
    return data_[17];
  }
  inline bool IsIsolatedWord(uint16 id) const {
    const uint16 offset = data_[35 + 17];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // SuggestOnlyWord "特殊,サジェストのみ"
  inline uint16 GetSuggestOnlyWordId() const {
    return data_[18];
  }
  inline bool IsSuggestOnlyWord(uint16 id) const {
    const uint16 offset = data_[35 + 18];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // ContentWordWithConjugation "^(動詞,自立,*,*,五段|動詞,自立,*,*,一段|形容詞,自立)"
  inline uint16 GetContentWordWithConjugationId() const {
    return data_[19];
  }
  inline bool IsContentWordWithConjugation(uint16 id) const {
    const uint16 offset = data_[35 + 19];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // SuffixWord "^(助詞|助動詞|動詞,非自立|動詞,接尾|形容詞,非自立|形容詞,接尾|動詞,自立,*,*,サ変・スル)"
  inline uint16 GetSuffixWordId() const {
    return data_[20];
  }
  inline bool IsSuffixWord(uint16 id) const {
    const uint16 offset = data_[35 + 20];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // CounterSuffixWord "名詞,接尾,助数詞"
  inline uint16 GetCounterSuffixWordId() const {
    return data_[21];
  }
  inline bool IsCounterSuffixWord(uint16 id) const {
    const uint16 offset = data_[35 + 21];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // UniqueNoun "^名詞,固有名詞"
  inline uint16 GetUniqueNounId() const {
    return data_[22];
  }
  inline bool IsUniqueNoun(uint16 id) const {
    const uint16 offset = data_[35 + 22];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // GeneralNoun "^名詞,一般,*,*,*,*,*$"
  inline uint16 GetGeneralNounId() const {
    return data_[23];
  }
  inline bool IsGeneralNoun(uint16 id) const {
    const uint16 offset = data_[35 + 23];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // Pronoun "^名詞,代名詞,"
  inline uint16 GetPronounId() const {
    return data_[24];
  }
  inline bool IsPronoun(uint16 id) const {
    const uint16 offset = data_[35 + 24];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // ContentNoun "^名詞,(一般|固有名詞|副詞可能|サ変接続),"
  inline uint16 GetContentNounId() const {
    return data_[25];
  }
  inline bool IsContentNoun(uint16 id) const {
    const uint16 offset = data_[35 + 25];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // NounPrefix "^接頭詞,名詞接続,"
  inline uint16 GetNounPrefixId() const {
    return data_[26];
  }
  inline bool IsNounPrefix(uint16 id) const {
    const uint16 offset = data_[35 + 26];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // EOSSymbol "^(記号,(句点|読点|アルファベット|一般|括弧開|括弧閉))|^(名詞,数,(アラビア数字|区切り文字))"
  inline uint16 GetEOSSymbolId() const {
    return data_[27];
  }
  inline bool IsEOSSymbol(uint16 id) const {
    const uint16 offset = data_[35 + 27];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // Adverb "^副詞,"
  inline uint16 GetAdverbId() const {
    return data_[28];
  }
  inline bool IsAdverb(uint16 id) const {
    const uint16 offset = data_[35 + 28];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // AdverbSegmentSuffix "^助詞,*,*,*,*,*,(から|で|と|に|にて|の|へ|を)$"
  inline uint16 GetAdverbSegmentSuffixId() const {
    return data_[29];
  }
  inline bool IsAdverbSegmentSuffix(uint16 id) const {
    const uint16 offset = data_[35 + 29];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // ParallelMarker "^助詞,並立助詞"
  inline uint16 GetParallelMarkerId() const {
    return data_[30];
  }
  inline bool IsParallelMarker(uint16 id) const {
    const uint16 offset = data_[35 + 30];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // TeSuffix "(助詞,接続助詞,*,*,*,*,(て|ちゃ)|助詞,並立助詞,*,*,*,*,たり|助詞,終助詞,*,*,*,*,てん|助動詞,*,*,*,特殊・タ,|動詞,非自立,*,*,一段,*,てる|助動詞,*,*,*,下二・タ行,連用形,つ|動詞,非自立,*,*,五段・カ行イ音便,*,とく|動詞,非自立,*,*,五段・カ行促音便,*,てく|動詞,非自立,*,*,五段・ラ行,*,(たる|とる)|動詞,非自立,*,*,五段・ワ行促音便,*,(ちゃう|ちまう)|動詞,非自立,*,*,一段,連用形,てる)"
  inline uint16 GetTeSuffixId() const {
    return data_[31];
  }
  inline bool IsTeSuffix(uint16 id) const {
    const uint16 offset = data_[35 + 31];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // VerbSuffix "(^動詞,非自立|^助詞,接続助詞|^助動詞)"
  inline uint16 GetVerbSuffixId() const {
    return data_[32];
  }
  inline bool IsVerbSuffix(uint16 id) const {
    const uint16 offset = data_[35 + 32];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // KagyoTaConnectionVerb "^動詞,(非自立|自立),*,*,五段・カ行(促音便|イ音便),連用タ接続"
  inline uint16 GetKagyoTaConnectionVerbId() const {
    return data_[33];
  }
  inline bool IsKagyoTaConnectionVerb(uint16 id) const {
    const uint16 offset = data_[35 + 33];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
  // WagyoRenyoConnectionVerb "^動詞,(非自立|自立),*,*,五段・ワ行促音便,連用形"
  inline uint16 GetWagyoRenyoConnectionVerbId() const {
    return data_[34];
  }
  inline bool IsWagyoRenyoConnectionVerb(uint16 id) const {
    const uint16 offset = data_[35 + 34];
    for (const uint16 *ptr = data_ + offset;
         *ptr != static_cast<uint16>(0xFFFF); ptr += 2) {
      if (id >= *ptr && id <= *(ptr + 1)) {
        return true;
      }
    }
    return false;
  }
 public:
  POSMatcher() : data_(nullptr) {}
  explicit POSMatcher(const uint16 *data) : data_(data) {}
  void Set(const uint16 *data) { data_ = data; }
 private:
  const uint16 *data_;
};
}  // namespace dictionary
}  // namespace mozc
#endif  // MOZC_DICTIONARY_POS_MATCHER_H_
