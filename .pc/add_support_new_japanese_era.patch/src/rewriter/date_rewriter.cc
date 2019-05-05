// Copyright 2010-2018, Google Inc.
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

// Date comment style are following.
//  - If the input number converts strictly 2 character with padding, comment
//  format is like "HH" or "MM".
//   e.g.) "YYYY/MM/DD HH:MM" ->  "2011/01/30 03:20"
//  - If the input number converts string without padding, comment format is
//  like "H" or "M"
//   e.g.) "Y/M/D H:M" -> "645/2/3 9:2"

#include "rewriter/date_rewriter.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "base/clock.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {

namespace {
struct DateData {
  const char *key;
  const char *value;
  const char *description;
  int diff;   // diff from the current time in day or month or year
};

const struct DateData kDateData[] = {
  {
    // きょう will show today's date
    "きょう",
    "今日",
    "今日の日付",
    0
  }, {
    // あした will show tomorrow's date
    "あした",
    "明日",
    "明日の日付",
    1
  }, {
    // あす will show tomorrow's date
    "あす",
    "明日",
    "明日の日付",
    1
  }, {
    // さくじつ will show yesterday's date
    "さくじつ",
    "昨日",
    "昨日の日付",
    -1
  }, {
    // きのう will show yesterday's date
    "きのう",
    "昨日",
    "昨日の日付",
    -1
  }, {
    // おととい will show the date of 2 days ago
    "おととい",
    "一昨日",
    "2日前の日付",
    -2
  }, {
    // おとつい will show the date of 2 days ago
    "おとつい",
    "一昨日",
    "2日前の日付",
    -2
  }, {
    // いっさくじつ will show the date of 2 days ago
    "いっさくじつ",
    "一昨日",
    "2日前の日付",
    -2
  }, {
    // さきおととい will show the date of 3 days ago
    "さきおととい",
    "一昨昨日",
    "3日前の日付",
    -3
  }, {
    // あさって will show the date of 2 days from now
    "あさって",
    "明後日",
    "明後日の日付",
    2,
  }, {
    // みょうごにち will show the date of 2 days from now
    "みょうごにち",
    "明後日",
    "明後日の日付",
    2
  }, {
    // しあさって will show the date of 3 days from now
    "しあさって",
    "明明後日",
    "明明後日の"
    "日付",
    3
  }
};

const struct DateData kWeekDayData[] = {
  {
    "にちようび",
    "日曜日",
    "次の日曜日",
    0
  }, {
    "げつようび",
    "月曜日",
    "次の月曜日",
    1
  }, {
    "かようび",
    "火曜日",
    "次の火曜日",
    2
  }, {
    "すいようび",
    "水曜日",
    "次の水曜日",
    3
  }, {
    "もくようび",
    "木曜日",
    "次の木曜日",
    4
  }, {
    "きんようび",
    "金曜日",
    "次の金曜日",
    5
  }, {
    "どようび",
    "土曜日",
    "次の土曜日",
    6
  }, {
    "にちよう",
    "日曜",
    "次の日曜日",
    0
  }, {
    "げつよう",
    "月曜",
    "次の月曜日",
    1
  }, {
    "かよう",
    "火曜",
    "次の火曜日",
    2
  }, {
    "すいよう",
    "水曜",
    "次の水曜日",
    3
  }, {
    "もくよう",
    "木曜",
    "次の木曜日",
    4
  }, {
    "きんよう",
    "金曜",
    "次の金曜日",
    5
  }, {
    "どよう",
    "土曜",
    "次の土曜日",
    6
  }
};

const struct DateData kYearData[] = {
  {
    "ことし",
    "今年",
    "今年",
    0
  }, {
    "らいねん",
    "来年",
    "来年",
    1
  }, {
    "さくねん",
    "昨年",
    "昨年",
    -1
  }, {
    "きょねん",
    "去年",
    "去年",
    -1
  }, {
    "おととし",
    "一昨年",
    "一昨年",
    -2
  }, {
    "さらいねん",
    "再来年",
    "再来年",
    2
  }
};

const struct DateData kMonthData[] = {
  {
    "こんげつ",
    "今月",
    "今月",
    0
  }, {
    "らいげつ",
    "来月",
    "来月",
    1
  }, {
    "せんげつ",
    "先月",
    "先月",
    -1
  }, {
    "せんせんげつ",
    "先々月",
    "先々月",
    -2
  }, {
    "さらいげつ",
    "再来月",
    "再来月",
    2
  }
};

const struct DateData kCurrentTimeData[] = {
  {
    "いま",
    "今",
    "現在の時刻",
    0
  }, {
    "じこく",
    "時刻",
    "現在の時刻",
    0
  }
};

const struct DateData kDateAndCurrentTimeData[] = {
  {
    "にちじ",
    "日時",
    "現在の日時",
    0
  },
};

struct YearData {
  int ad;      // AD year
  const char *era;   // Japanese year a.k.a, GENGO
  const char *key;   // reading of the `era`
};

const YearData kEraData[] = {
  // "元徳", "建武" and "明徳" are used for both south and north courts.
  {
    645,
    "大化",
    "たいか",
  }, {
    650,
    "白雉",
    "はくち",
  }, {
    686,
    "朱鳥",
    "しゅちょう",
  }, {
    701,
    "大宝",
    "たいほう",
  }, {
    704,
    "慶雲",
    "けいうん",
  }, {
    708,
    "和銅",
    "わどう",
  }, {
    715,
    "霊亀",
    "れいき",
  }, {
    717,
    "養老",
    "ようろう",
  }, {
    724,
    "神亀",
    "じんき",
  }, {
    729,
    "天平",
    "てんぴょう",
  }, {
    749,
    "天平感宝",
    "てんぴょう"
    "かんぽう",
  }, {
    749,
    "天平勝宝",
    "てんぴょう"
    "しょうほう",
  }, {
    757,
    "天平宝字",
    "てんぴょう"
    "ほうじ",
  }, {
    765,
    "天平神護",
    "てんぴょう"
    "じんご",
  }, {
    767,
    "神護景雲",
    "じんご"
    "けいうん",
  }, {
    770,
    "宝亀",
    "ほうき",
  }, {
    781,
    "天応",
    "てんおう",
  }, {
    782,
    "延暦",
    "えんりゃく",
  }, {
    806,
    "大同",
    "たいどう",
  }, {
    810,
    "弘仁",
    "こうにん",
  }, {
    824,
    "天長",
    "てんちょう",
  }, {
    834,
    "承和",
    "じょうわ",
  }, {
    848,
    "嘉祥",
    "かしょう",
  }, {
    851,
    "仁寿",
    "にんじゅ",
  }, {
    854,
    "斉衡",
    "さいこう",
  }, {
    857,
    "天安",
    "てんなん",
  }, {
    859,
    "貞観",
    "じょうかん",
  }, {
    877,
    "元慶",
    "がんぎょう",
  }, {
    885,
    "仁和",
    "にんな",
  }, {
    889,
    "寛平",
    "かんぴょう",
  }, {
    898,
    "昌泰",
    "しょうたい",
  }, {
    901,
    "延喜",
    "えんぎ",
  }, {
    923,
    "延長",
    "えんちょう",
  }, {
    931,
    "承平",
    "じょうへい",
  }, {
    938,
    "天慶",
    "てんぎょう",
  }, {
    947,
    "天暦",
    "てんりゃく",
  }, {
    957,
    "天徳",
    "てんとく",
  }, {
    961,
    "応和",
    "おうわ",
  }, {
    964,
    "康保",
    "こうほう",
  }, {
    968,
    "安和",
    "あんな",
  }, {
    970,
    "天禄",
    "てんろく",
  }, {
    973,
    "天延",
    "てんえん",
  }, {
    976,
    "貞元",
    "じょうげん",
  }, {
    978,
    "天元",
    "てんげん",
  }, {
    983,
    "永観",
    "えいかん",
  }, {
    985,
    "寛和",
    "かんな",
  }, {
    987,
    "永延",
    "えいえん",
  }, {
    989,
    "永祚",
    "えいそ",
  }, {
    990,
    "正暦",
    "しょうりゃく",
  }, {
    995,
    "長徳",
    "ちょうとく",
  }, {
    999,
    "長保",
    "ちょうほう",
  }, {
    1004,
    "寛弘",
    "かんこう",
  }, {
    1012,
    "長和",
    "ちょうわ",
  }, {
    1017,
    "寛仁",
    "かんにん",
  }, {
    1021,
    "治安",
    "じあん",
  }, {
    1024,
    "万寿",
    "まんじゅ",
  }, {
    1028,
    "長元",
    "ちょうげん",
  }, {
    1037,
    "長暦",
    "ちょうりゃく",
  }, {
    1040,
    "長久",
    "ちょうきゅう",
  }, {
    1044,
    "寛徳",
    "かんとく",
  }, {
    1046,
    "永承",
    "えいしょう",
  }, {
    1053,
    "天喜",
    "てんき",
  }, {
    1058,
    "康平",
    "こうへい",
  }, {
    1065,
    "治暦",
    "じりゃく",
  }, {
    1069,
    "延久",
    "えんきゅう",
  }, {
    1074,
    "承保",
    "じょうほう",
  }, {
    1077,
    "承暦",
    "じょうりゃく",
  }, {
    1081,
    "永保",
    "えいほ",
  }, {
    1084,
    "応徳",
    "おうとく",
  }, {
    1087,
    "寛治",
    "かんじ",
  }, {
    1094,
    "嘉保",
    "かほう",
  }, {
    1096,
    "永長",
    "えいちょう",
  }, {
    1097,
    "承徳",
    "じょうとく",
  }, {
    1099,
    "康和",
    "こうわ",
  }, {
    1104,
    "長治",
    "ちょうじ",
  }, {
    1106,
    "嘉承",
    "かしょう",
  }, {
    1108,
    "天仁",
    "てんにん",
  }, {
    1110,
    "天永",
    "てんえい",
  }, {
    1113,
    "永久",
    "えいきゅう",
  }, {
    1118,
    "元永",
    "げんえい",
  }, {
    1120,
    "保安",
    "ほうあん",
  }, {
    1124,
    "天治",
    "てんじ",
  }, {
    1126,
    "大治",
    "だいじ",
  }, {
    1131,
    "天承",
    "てんじょう",
  }, {
    1132,
    "長承",
    "ちょうじょう",
  }, {
    1135,
    "保延",
    "ほうえん",
  }, {
    1141,
    "永治",
    "えいじ",
  }, {
    1142,
    "康治",
    "こうじ",
  }, {
    1144,
    "天養",
    "てんよう",
  }, {
    1145,
    "久安",
    "きゅうあん",
  }, {
    1151,
    "仁平",
    "にんぺい",
  }, {
    1154,
    "久寿",
    "きゅうじゅ",
  }, {
    1156,
    "保元",
    "ほうげん",
  }, {
    1159,
    "平治",
    "へいじ",
  }, {
    1160,
    "永暦",
    "えいりゃく",
  }, {
    1161,
    "応保",
    "おうほ",
  }, {
    1163,
    "長寛",
    "ちょうかん",
  }, {
    1165,
    "永万",
    "えいまん",
  }, {
    1166,
    "仁安",
    "にんあん",
  }, {
    1169,
    "嘉応",
    "かおう",
  }, {
    1171,
    "承安",
    "しょうあん",
  }, {
    1175,
    "安元",
    "あんげん",
  }, {
    1177,
    "治承",
    "じしょう",
  }, {
    1181,
    "養和",
    "ようわ",
  }, {
    1182,
    "寿永",
    "じゅえい",
  }, {
    1184,
    "元暦",
    "げんりゃく",
  }, {
    1185,
    "文治",
    "ぶんじ",
  }, {
    1190,
    "建久",
    "けんきゅう",
  }, {
    1199,
    "正治",
    "しょうじ",
  }, {
    1201,
    "建仁",
    "けんにん",
  }, {
    1204,
    "元久",
    "げんきゅう",
  }, {
    1206,
    "建永",
    "けんえい",
  }, {
    1207,
    "承元",
    "じょうげん",
  }, {
    1211,
    "建暦",
    "けんりゃく",
  }, {
    1213,
    "建保",
    "けんぽう",
  }, {
    1219,
    "承久",
    "しょうきゅう",
  }, {
    1222,
    "貞応",
    "じょうおう",
  }, {
    1224,
    "元仁",
    "げんにん",
  }, {
    1225,
    "嘉禄",
    "かろく",
  }, {
    1227,
    "安貞",
    "あんてい",
  }, {
    1229,
    "寛喜",
    "かんき",
  }, {
    1232,
    "貞永",
    "じょうえい",
  }, {
    1233,
    "天福",
    "てんぷく",
  }, {
    1234,
    "文暦",
    "ぶんりゃく",
  }, {
    1235,
    "嘉禎",
    "かてい",
  }, {
    1238,
    "暦仁",
    "りゃくにん",
  }, {
    1239,
    "延応",
    "えんおう",
  }, {
    1240,
    "仁治",
    "にんじゅ",
  }, {
    1243,
    "寛元",
    "かんげん",
  }, {
    1247,
    "宝治",
    "ほうじ",
  }, {
    1249,
    "建長",
    "けんちょう",
  }, {
    1256,
    "康元",
    "こうげん",
  }, {
    1257,
    "正嘉",
    "しょうか",
  }, {
    1259,
    "正元",
    "しょうげん",
  }, {
    1260,
    "文応",
    "ぶんおう",
  }, {
    1261,
    "弘長",
    "こうちょう",
  }, {
    1264,
    "文永",
    "ぶんえい",
  }, {
    1275,
    "建治",
    "けんじ",
  }, {
    1278,
    "弘安",
    "こうあん",
  }, {
    1288,
    "正応",
    "しょうおう",
  }, {
    1293,
    "永仁",
    "えいにん",
  }, {
    1299,
    "正安",
    "しょうあん",
  }, {
    1302,
    "乾元",
    "けんげん",
  }, {
    1303,
    "嘉元",
    "かげん",
  }, {
    1306,
    "徳治",
    "とくじ",
  }, {
    1308,
    "延慶",
    "えんぎょう",
  }, {
    1311,
    "応長",
    "おうちょう",
  }, {
    1312,
    "正和",
    "しょうわ",
  }, {
    1317,
    "文保",
    "ぶんぽう",
  }, {
    1319,
    "元応",
    "げんおう",
  }, {
    1321,
    "元亨",
    "げんこう",
  }, {
    1324,
    "正中",
    "しょうちゅう",
  }, {
    1326,
    "嘉暦",
    "かりゃく",
  }, {
    1329,
    "元徳",
    "げんとく",
  }, {
    1331,
    "元弘",
    "げんこう",
  }, {
    1334,
    "建武",
    "けんむ",
  }, {
    1336,
    "延元",
    "えんげん",
  }, {
    1340,
    "興国",
    "こうこく",
  }, {
    1346,
    "正平",
    "しょうへい",
  }, {
    1370,
    "建徳",
    "けんとく",
  }, {
    1372,
    "文中",
    "ぶんちゅう",
  }, {
    1375,
    "天授",
    "てんじゅ",
  }, {
    1381,
    "弘和",
    "こうわ",
  }, {
    1384,
    "元中",
    "げんちゅう",
  }, {
    1390,
    "明徳",
    "めいとく",
  }, {
    1394,
    "応永",
    "おうえい",
  }, {
    1428,
    "正長",
    "しょうちょう",
  }, {
    1429,
    "永享",
    "えいきょう",
  }, {
    1441,
    "嘉吉",
    "かきつ",
  }, {
    1444,
    "文安",
    "ぶんあん",
  }, {
    1449,
    "宝徳",
    "ほうとく",
  }, {
    1452,
    "享徳",
    "きょうとく",
  }, {
    1455,
    "康正",
    "こうしょう",
  }, {
    1457,
    "長禄",
    "ちょうろく",
  }, {
    1460,
    "寛正",
    "かんしょう",
  }, {
    1466,
    "文正",
    "ぶんしょう",
  }, {
    1467,
    "応仁",
    "おうにん",
  }, {
    1469,
    "文明",
    "ぶんめい",
  }, {
    1487,
    "長享",
    "ちょうきょう",
  }, {
    1489,
    "延徳",
    "えんとく",
  }, {
    1492,
    "明応",
    "めいおう",
  }, {
    1501,
    "文亀",
    "ぶんき",
  }, {
    1504,
    "永正",
    "えいしょう",
  }, {
    1521,
    "大永",
    "だいえい",
  }, {
    1528,
    "享禄",
    "きょうろく",
  }, {
    1532,
    "天文",
    "てんぶん",
  }, {
    1555,
    "弘治",
    "こうじ",
  }, {
    1558,
    "永禄",
    "えいろく",
  }, {
    1570,
    "元亀",
    "げんき",
  }, {
    1573,
    "天正",
    "てんしょう",
  }, {
    1592,
    "文禄",
    "ぶんろく",
  }, {
    1596,
    "慶長",
    "けいちょう",
  }, {
    1615,
    "元和",
    "げんな",
  }, {
    1624,
    "寛永",
    "かんえい",
  }, {
    1644,
    "正保",
    "しょうほう",
  }, {
    1648,
    "慶安",
    "けいあん",
  }, {
    1652,
    "承応",
    "じょうおう",
  }, {
    1655,
    "明暦",
    "めいれき",
  }, {
    1658,
    "万治",
    "まんじ",
  }, {
    1661,
    "寛文",
    "かんぶん",
  }, {
    1673,
    "延宝",
    "えんぽう",
  }, {
    1681,
    "天和",
    "てんな",
  }, {
    1684,
    "貞享",
    "じょうきょう",
  }, {
    1688,
    "元禄",
    "げんろく",
  }, {
    1704,
    "宝永",
    "ほうえい",
  }, {
    1711,
    "正徳",
    "しょうとく",
  }, {
    1716,
    "享保",
    "きょうほ",
  }, {
    1736,
    "元文",
    "げんぶん",
  }, {
    1741,
    "寛保",
    "かんぽ",
  }, {
    1744,
    "延享",
    "えんきょう",
  }, {
    1748,
    "寛延",
    "かんえん",
  }, {
    1751,
    "宝暦",
    "ほうれき",
  }, {
    1764,
    "明和",
    "めいわ",
  }, {
    1772,
    "安永",
    "あんえい",
  }, {
    1781,
    "天明",
    "てんめい",
  }, {
    1789,
    "寛政",
    "かんせい",
  }, {
    1801,
    "享和",
    "きょうわ",
  }, {
    1804,
    "文化",
    "ぶんか",
  }, {
    1818,
    "文政",
    "ぶんせい",
  }, {
    1830,
    "天保",
    "てんぽう",
  }, {
    1844,
    "弘化",
    "こうか",
  }, {
    1848,
    "嘉永",
    "かえい",
  }, {
    1854,
    "安政",
    "あんせい",
  }, {
    1860,
    "万延",
    "まんえん",
  }, {
    1861,
    "文久",
    "ぶんきゅう",
  }, {
    1864,
    "元治",
    "げんじ",
  }, {
    1865,
    "慶応",
    "けいおう",
  }, {
    1868,
    "明治",
    "めいじ",
  }, {
    1912,
    "大正",
    "たいしょう",
  }, {
    1926,
    "昭和",
    "しょうわ",
  }, {
    1989,
    "平成",
    "へいせい",
  }
};

const YearData kNorthEraData[] = {
  // "元徳", "建武" and "明徳" are used for both south and north courts.
  {
    1329,
    "元徳",
    "げんとく",
  }, {
    1332,
    "正慶",
    "しょうけい",
  }, {
    1334,
    "建武",
    "けんむ",
  }, {
    1338,
    "暦応",
    "りゃくおう",
  }, {
    1342,
    "康永",
    "こうえい",
  }, {
    1345,
    "貞和",
    "じょうわ",
  }, {
    1350,
    "観応",
    "かんおう",
  }, {
    1352,
    "文和",
    "ぶんわ",
  }, {
    1356,
    "延文",
    "えんぶん",
  }, {
    1361,
    "康安",
    "こうあん",
  }, {
    1362,
    "貞治",
    "じょうじ",
  }, {
    1368,
    "応安",
    "おうあん",
  }, {
    1375,
    "永和",
    "えいわ",
  }, {
    1379,
    "康暦",
    "こうりゃく",
  }, {
    1381,
    "永徳",
    "えいとく",
  }, {
    1384,
    "至徳",
    "しとく",
  }, {
    1387,
    "嘉慶",
    "かけい",
  }, {
    1389,
    "康応",
    "こうおう",
  }, {
    1390,
    "明徳",
    "めいとく",
  }
};

const char *kWeekDayString[] = {
  "日",
  "月",
  "火",
  "水",
  "木",
  "金",
  "土",
};

const char kDateDescription[] = "日付";
const char kTimeDescription[] = "時刻";

bool PrintUint32(const char *format, uint32 num, char *buf, size_t buf_size) {
  const int ret = std::snprintf(buf, buf_size, format, num);
  return 0 <= ret && ret < buf_size;
}

// Helper function to generate "H時M分" time formats.
void GenerateKanjiTimeFormats(
    const char *hour_format, const char *min_format, uint32 hour, uint32 min,
    std::vector<std::pair<string, const char *>> *results) {
  char hour_s[4], min_s[4];
  if (!PrintUint32(hour_format, hour, hour_s, 4) ||
      !PrintUint32(min_format, min, min_s, 4)) {
    return;
  }
  results->emplace_back(
      Util::StringPrintf("%s時%s分", hour_s, min_s),
      kTimeDescription);
  // "H時半".  Don't generate it when the printed hour starts with 0 because
  // formats like "03時半" is rarely used (but "3時半" is ok).
  if (hour_s[0] != '0' && min == 30) {
    results->emplace_back(
        Util::StringPrintf("%s時半", hour_s),
        kTimeDescription);
  }
}

// Helper function to generate "午前..." and "午後..." time formats.
void GenerateGozenGogoTimeFormats(
    const char *hour_format, const char *min_format, uint32 hour, uint32 min,
    std::vector<std::pair<string, const char *>> *results) {
  // "午前" and "午後" prefixes are only used for [0, 11].
  if (hour >= 12) {
    return;
  }
  char hour_s[4], min_s[4];
  if (!PrintUint32(hour_format, hour, hour_s, 4) ||
      !PrintUint32(min_format, min, min_s, 4)) {
    return;
  }
  results->emplace_back(Util::StringPrintf("午前%s時%s分", hour_s, min_s),
                        kTimeDescription);
  if (min == 30) {
    results->emplace_back(Util::StringPrintf("午前%s時半", hour_s),
                          kTimeDescription);
  }

  results->emplace_back(Util::StringPrintf("午後%s時%s分", hour_s, min_s),
                        kTimeDescription);
  if (min == 30) {
    results->emplace_back(Util::StringPrintf("午後%s時半", hour_s),
                          kTimeDescription);
  }
}

// Converts a prefix and year number to Japanese Kanji Representation
// arguments :
//      - input "prefix" : a japanese style year counter prefix.
//      - input "year" : represent integer must be smaller than 100.
//      - output "result" : will be stored candidate strings
// If the input year is invalid ( accept only [ 1 - 99 ] ) , this function
// returns false and clear output vector.
bool ExpandYear(const string &prefix, int year, std::vector<string> *result) {
  DCHECK(result);
  if (year <= 0 || year >= 100) {
    result->clear();
    return false;
  }

  if (year == 1) {
    //  "元年"
    result->push_back(prefix + "元");
    return true;
  }

  result->push_back(prefix + std::to_string(year));

  string arabic = std::to_string(year);

  std::vector<NumberUtil::NumberString> output;

  NumberUtil::ArabicToKanji(arabic, &output);

  for (int i = 0; i < output.size(); i++) {
    if (output[i].style == NumberUtil::NumberString::NUMBER_KANJI) {
      result->push_back(prefix + output[i].value);
    }
  }

  return true;
}

void Insert(const Segment::Candidate &base_candidate,
            int position,
            const string &value,
            const char *description,
            Segment *segment) {
  Segment::Candidate *c = segment->insert_candidate(position);
  DCHECK(c);
  c->Init();
  c->lid = base_candidate.lid;
  c->rid = base_candidate.rid;
  c->cost = base_candidate.cost;
  c->value = value;
  c->key = base_candidate.key;
  c->content_key = base_candidate.content_key;
  c->attributes |= Segment::Candidate::NO_LEARNING;
  c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  if (description != nullptr) {
    c->description = description;
  }
}

enum {
  REWRITE_YEAR,
  REWRITE_DATE,
  REWRITE_MONTH,
  REWRITE_CURRENT_TIME,
  REWRITE_DATE_AND_CURRENT_TIME
};

bool AdToEraForCourt(const YearData *data, int size,
                     int year, std::vector<string> *results) {
  for (int i = size - 1; i >= 0; --i) {
    if (i == size - 1 && year > data[i].ad) {
      ExpandYear(data[i].era, year - data[i].ad + 1, results);
      return true;
    } else if (i > 0 && data[i-1].ad < year && year <= data[i].ad) {
      // have two representations:
      // 1989 -> "昭和64" and "平成元"
      if (year == data[i].ad) {
        ExpandYear(data[i].era, 1, results);
      }
      ExpandYear(data[i - 1].era, year - data[i - 1].ad + 1, results);
      return true;
    } else if (i == 0 && data[i].ad <= year) {
      if (year == data[i].ad) {
        ExpandYear(data[i].era, 1, results);
      } else {
        ExpandYear(data[i - 1].era, year - data[i - 1].ad + 1, results);
      }
      return true;
    }
  }

  return false;
}

const char kNenKey[] = "ねん";
const char kNenValue[] = "年";

bool ExtractYearFromKey(const YearData &year_data,
                        const string &key,
                        int *year,
                        string *description) {
  const char kGanKey[] = "がん";
  const char kGanValue[] = "元";

  // Util::EndsWith(key, kNenKey) is expected to always return true
  DCHECK(Util::EndsWith(key, kNenKey));
  if (!Util::StartsWith(key, year_data.key)) {
    return false;
  }
  // key="しょうわ59ねん" -> era_year_str="59"
  // key="へいせいがんねん" -> era_year_str="がん"
  const size_t year_start = Util::CharsLen(year_data.key);
  const size_t year_length = Util::CharsLen(key) -
      year_start - Util::CharsLen(kNenKey);
  string era_year_str(Util::SubString(key, year_start, year_length));

  if (era_year_str == kGanKey) {
    *year = 1;
    description->assign(year_data.era).append(kGanValue).append(kNenValue);
    return true;
  }

  if (!NumberUtil::IsArabicNumber(era_year_str)) {
    return false;
  }
  *year = NumberUtil::SimpleAtoi(era_year_str);
  if (*year <= 0) {
    return false;
  }
  *description = year_data.era + era_year_str + kNenValue;

  return true;
}

bool EraToAdForCourt(const YearData *data, size_t size, const string &key,
                     std::vector<string> *results,
                     std::vector<string> *descriptions) {
  if (!Util::EndsWith(key, kNenKey)) {
    return false;
  }

  bool modified = false;
  for (size_t i = 0; i < size; ++i) {
    const YearData &year_data = data[i];
    if (!Util::StartsWith(key, year_data.key)) {
      continue;
    }

    // key="しょうわ59ねん" -> era_year=59, description="昭和59年"
    // key="へいせいがんねん" -> era_year=1, description="平成元年"
    int era_year = 0;
    string description = "";
    if (!ExtractYearFromKey(year_data, key, &era_year, &description)) {
      continue;
    }
    const int ad_year = era_year + year_data.ad - 1;

    // Get wide arabic numbers
    // e.g.) 1989 -> "１９８９", "一九八九"
    std::vector<NumberUtil::NumberString> output;
    const string ad_year_str(std::to_string(ad_year));
    NumberUtil::ArabicToWideArabic(ad_year_str, &output);
    // add half-width arabic number to `output` (e.g. "1989")
    output.push_back(NumberUtil::NumberString(
        ad_year_str,
        "",
        NumberUtil::NumberString::DEFAULT_STYLE));

    for (size_t j = 0; j < output.size(); ++j) {
      // "元徳", "建武" and "明徳" require dedupe
      const string value(output[j].value + kNenValue);
      std::vector<string>::const_iterator found =
          std::find(results->begin(), results->end(), value);
      if (found != results->end()) {
        continue;
      }
      results->push_back(value);
      descriptions->push_back(description);
    }
    modified = true;
  }
  return modified;
}

// Checkes if the given date is valid or not.
// Over 24 hour expression is allowed in this function.
// Acceptable hour is between 0 and 29.
bool IsValidTime(uint32 hour, uint32 minute) {
  return hour < 30 && minute < 60;
}

// Returns February last day.
// This function deals with leap year with Gregorian calendar.
uint32 GetFebruaryLastDay(uint32 year) {
  uint32 february_end = (year % 4 == 0) ? 29 : 28;
  if (year % 100 == 0 && year % 400 != 0) {
    february_end = 28;
  }
  return february_end;
}

// Checkes given date is valid or not.
bool IsValidDate(uint32 year, uint32 month, uint32 day) {
  if (day < 1) {
    return false;
  }

  if (year == 0 || year > 2100) {
    return false;
  }

  switch (month) {
    case 2: {
      if (day > GetFebruaryLastDay(year)) {
        return false;
      } else {
        return true;
      }
    }
    case 4: case 6: case 9: case 11: {
      if (day > 30) {
        return false;
      } else {
        return true;
      }
    }
    case 1: case 3: case 5: case 7: case 8: case 10: case 12: {
      if (day > 31) {
        return false;
      } else {
        return true;
      }
    }
    default: {
      return false;
    }
  }
}

// Checks if a pair of month and day is valid.
bool IsValidMonthAndDay(uint32 month, uint32 day) {
  if (day == 0) {
    return false;
  }
  switch (month) {
    case 2:
      return day <= 29;
    case 4: case 6: case 9: case 11:
      return day <= 30;
    case 1: case 3: case 5: case 7: case 8: case 10: case 12:
      return day <= 31;
    default:
      return false;
  }
}

}  // namespace

// convert AD to Japanese ERA.
// The results will have multiple variants.
bool DateRewriter::AdToEra(int year, std::vector<string> *results) {
  if (year < 645 || year > 2050) {    // TODO(taku) is it enough?
    return false;
  }

  // The order is south to north.
  std::vector<string> eras;
  bool r = false;
  r = AdToEraForCourt(kEraData, arraysize(kEraData), year, &eras);
  if (year > 1331 && year < 1393) {
    r |= AdToEraForCourt(kNorthEraData, arraysize(kNorthEraData),
                          year, &eras);
  }
  // 1334 requires dedupe
  for (int i = 0; i < eras.size(); ++i) {
    bool found = false;
    for (int j = 0; j < i; ++j) {
      if (eras[j] == eras[i]) {
        found = true;
      }
    }
    if (!found) {
      results->push_back(eras[i]);
    }
  }
  return r;
}

bool DateRewriter::EraToAd(const string &key,
                           std::vector<string> *results,
                           std::vector<string> *descriptions) {
  bool ret = false;
  // The order is south to north, older to newer
  ret |= EraToAdForCourt(kEraData, arraysize(kEraData),
                         key, results, descriptions);
  ret |= EraToAdForCourt(kNorthEraData, arraysize(kNorthEraData),
                         key, results, descriptions);
  return ret;
}

bool DateRewriter::ConvertTime(uint32 hour, uint32 min,
                               std::vector<string> *results) {
  DCHECK(results);
  if (!IsValidTime(hour, min)) {
    return false;
  }
  results->push_back(Util::StringPrintf("%d:%2.2d", hour, min));
  results->push_back(Util::StringPrintf("%d時%2.2d分", hour, min));
  if (min == 30) {
    results->push_back(Util::StringPrintf("%d時半", hour));
  }

  if ((hour % 24) * 60 + min < 720) {  // 0:00 -- 11:59
    results->push_back(Util::StringPrintf("午前%d時%d分", hour % 24, min));
    if (min == 30) {
      results->push_back(Util::StringPrintf("午前%d時半", hour % 24));
    }
  } else {
    results->push_back(
        Util::StringPrintf("午後%d時%d分", (hour - 12) % 24, min));
    if (min == 30) {
      results->push_back(Util::StringPrintf("午後%d時半", (hour - 12) % 24));
    }
  }
  return true;
}

bool DateRewriter::ConvertDateWithYear(uint32 year, uint32 month, uint32 day,
                                       std::vector<string> *results) {
  DCHECK(results);
  if (!IsValidDate(year, month, day)) {
    return false;
  }
  // Generate "Y/MM/DD", "Y-MM-DD" and "Y年M月D日" formats.
  results->push_back(Util::StringPrintf("%d/%2.2d/%2.2d", year, month, day));
  results->push_back(Util::StringPrintf("%d-%2.2d-%2.2d", year, month, day));
  results->push_back(Util::StringPrintf("%d年%d月%d日", year, month, day));
  return true;
}

bool DateRewriter::RewriteTime(Segment *segment,
                               const char *key,
                               const char *value,
                               const char *description,
                               int type, int diff) {
  if (segment->key() != key) {   // only exact match
    return false;
  }

  const size_t kMinSize = 10;
  const size_t size = std::min(kMinSize, segment->candidates_size());

  for (size_t cand_idx = 0; cand_idx < size; ++cand_idx) {
    const Segment::Candidate &cand = segment->candidate(cand_idx);
    if (cand.value != value) {
      continue;
    }
    // Date candidates are too many, therefore highest candidate show at most
    // 3rd.
    // TODO(nona): learn date candidate even if the date is changed.
    const size_t kMinimumDateCandidateIdx = 3;
    const size_t insert_idx =
        (size < kMinimumDateCandidateIdx)
            ? size
            : std::max(cand_idx + 1, kMinimumDateCandidateIdx);

    struct tm t_st;
    std::vector<string> era;
    switch (type) {
      case REWRITE_DATE: {
        if (!Clock::GetTmWithOffsetSecond(&t_st, diff * 86400)) {
          LOG(ERROR) << "GetTmWithOffsetSecond() failed";
          return false;
        }
        std::vector<string> results;
        ConvertDateWithYear(t_st.tm_year + 1900, t_st.tm_mon + 1, t_st.tm_mday,
                            &results);
        if (AdToEra(t_st.tm_year + 1900, &era) && !era.empty()) {
          results.push_back(Util::StringPrintf(
              "%s年%d月%d日",
              era[0].c_str(), t_st.tm_mon + 1, t_st.tm_mday));
        }
        results.push_back(Util::StringPrintf("%s曜日",
                                             kWeekDayString[t_st.tm_wday]));

        for (std::vector<string>::reverse_iterator rit = results.rbegin();
             rit != results.rend(); ++rit) {
          Insert(cand, insert_idx , *rit, description, segment);
        }
        return true;
      }

      case REWRITE_MONTH: {
        if (!Clock::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        const int month = (t_st.tm_mon + diff + 12) % 12 + 1;
        Insert(cand, insert_idx, Util::StringPrintf("%d月", month),
               description, segment);
        Insert(cand, insert_idx, Util::StringPrintf("%d", month), description,
               segment);
        return true;
      }

      case REWRITE_YEAR: {
        if (!Clock::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        const int year = (t_st.tm_year + diff + 1900);
        if (AdToEra(year, &era) && !era.empty()) {
          Insert(cand, insert_idx,
                 Util::StringPrintf("%s年", era[0].c_str()),
                 description, segment);
        }
        Insert(cand, insert_idx, Util::StringPrintf("%d年", year),
               description, segment);
        Insert(cand, insert_idx, Util::StringPrintf("%d", year), description,
               segment);
        return true;
      }

      case REWRITE_CURRENT_TIME: {
        if (!Clock::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        std::vector<string> times;
        ConvertTime(t_st.tm_hour, t_st.tm_min, &times);
        for (std::vector<string>::reverse_iterator rit = times.rbegin();
             rit != times.rend(); ++rit) {
          Insert(cand, insert_idx, *rit, description, segment);
        }
        return true;
      }

      case REWRITE_DATE_AND_CURRENT_TIME: {
        if (!Clock::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        // Y/MM/DD H:MM
        const string ymmddhmm = Util::StringPrintf("%d/%2.2d/%2.2d %2d:%2.2d",
                                                   t_st.tm_year + 1900,
                                                   t_st.tm_mon + 1,
                                                   t_st.tm_mday,
                                                   t_st.tm_hour,
                                                   t_st.tm_min);
        Insert(cand, insert_idx, ymmddhmm, description, segment);
        return true;
      }
    }
  }

  return false;
}

bool DateRewriter::RewriteDate(Segment *segment) {
  for (size_t i = 0; i < arraysize(kDateData); ++i) {
    if (RewriteTime(segment,
                    kDateData[i].key, kDateData[i].value,
                    kDateData[i].description,
                    REWRITE_DATE,
                    kDateData[i].diff)) {
      VLOG(1) << "RewriteDate: "
              << kDateData[i].key << " " << kDateData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteMonth(Segment *segment) {
  for (size_t i = 0; i < arraysize(kMonthData); ++i) {
    if (RewriteTime(segment,
                    kMonthData[i].key, kMonthData[i].value,
                    kMonthData[i].description,
                    REWRITE_MONTH,
                    kMonthData[i].diff)) {
      VLOG(1) << "RewriteMonth: "
              << kMonthData[i].key << " " << kMonthData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteYear(Segment *segment) {
  for (size_t i = 0; i < arraysize(kYearData); ++i) {
    if (RewriteTime(segment,
                    kYearData[i].key, kYearData[i].value,
                    kYearData[i].description,
                    REWRITE_YEAR,
                    kYearData[i].diff)) {
      VLOG(1) << "RewriteYear: "
              << kYearData[i].key << " " << kYearData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteWeekday(Segment *segment) {
  struct tm t_st;
  if (!Clock::GetCurrentTm(&t_st)) {
    LOG(ERROR) << "GetCurrentTm failed";
    return false;
  }

  for (size_t i = 0; i < arraysize(kWeekDayData); ++i) {
    const int weekday = kWeekDayData[i].diff % 7;
    const int additional_dates = (weekday + 7 - t_st.tm_wday) % 7;
    if (RewriteTime(segment,
                    kWeekDayData[i].key, kWeekDayData[i].value,
                    kWeekDayData[i].description,
                    REWRITE_DATE,
                    additional_dates)) {
      VLOG(1) << "RewriteWeekday: "
              << kWeekDayData[i].key << " " << kWeekDayData[i].value;
      return true;
    }
  }

  return false;
}

bool DateRewriter::RewriteCurrentTime(Segment *segment) {
  for (size_t i = 0; i < arraysize(kCurrentTimeData); ++i) {
    if (RewriteTime(segment,
                    kCurrentTimeData[i].key, kCurrentTimeData[i].value,
                    kCurrentTimeData[i].description,
                    REWRITE_CURRENT_TIME,
                    0)) {
      VLOG(1) << "RewriteCurrentTime: "
              << kCurrentTimeData[i].key << " "
              << kCurrentTimeData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteDateAndCurrentTime(Segment *segment) {
  for (size_t i = 0; i < arraysize(kDateAndCurrentTimeData); ++i) {
    if (RewriteTime(segment,
                    kDateAndCurrentTimeData[i].key,
                    kDateAndCurrentTimeData[i].value,
                    kDateAndCurrentTimeData[i].description,
                    REWRITE_DATE_AND_CURRENT_TIME,
                    0)) {
      VLOG(1) << "RewriteDateAndCurrentTime: "
              << kDateAndCurrentTimeData[i].key << " "
              << kDateAndCurrentTimeData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteEra(Segment *current_segment,
                              const Segment &next_segment) {
  if (current_segment->candidates_size() <= 0 ||
      next_segment.candidates_size() <= 0) {
    LOG(ERROR) << "Candidate size is 0";
    return false;
  }

  const string &current_key = current_segment->key();
  const string &next_value = next_segment.candidate(0).value;

  if (next_value != "年") {
    return false;
  }

  if (Util::GetScriptType(current_key) != Util::NUMBER) {
    return false;
  }

  const size_t len = Util::CharsLen(current_key);
  if (len < 3 || len > 4) {
    LOG(WARNING) << "Too long year";
    return false;
  }

  string year_str;
  Util::FullWidthAsciiToHalfWidthAscii(current_key, &year_str);

  uint32 year = 0;
  if (!NumberUtil::SafeStrToUInt32(year_str, &year)) {
    return false;
  }

  std::vector<string> results;
  if (!AdToEra(year, &results)) {
    return false;
  }

  const int kInsertPosition = 2;
  const int position = std::min(
      kInsertPosition, static_cast<int>(current_segment->candidates_size()));

  const char kDescription[] = "和暦";
  for (int i = static_cast<int>(results.size()) - 1; i >= 0; --i) {
    Insert(current_segment->candidate(0),
           position,
           results[i],
           kDescription,
           current_segment);
    current_segment->mutable_candidate(position)->attributes
        &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
  }

  return true;
}

bool DateRewriter::RewriteAd(Segment *segment) {
  const string &key = segment->key();
  if (!Util::EndsWith(key, kNenKey)) {
    return false;
  }
  if (segment->candidates_size() == 0) {
    VLOG(2) << "No candidates are found";
    return false;
  }
  std::vector<string> results, descriptions;
  const bool ret = EraToAd(key, &results, &descriptions);

  // Insert position is the last of candidates
  const int position = static_cast<int>(segment->candidates_size());
  for (size_t i = 0; i < results.size(); ++i)  {
    Insert(segment->candidate(0),
           position,
           results[i],
           descriptions[i].c_str(),
           segment);
  }
  return ret;
}

namespace {
bool IsNDigits(const string &value, int n) {
  return Util::CharsLen(value) == n &&
         Util::GetScriptType(value) == Util::NUMBER;
}

// Gets n digits if possible.
// Following trials will be performed in this order.
// 1. Checks segment's key.
// 2. Checks all the meta candidates.
// 3. Checks raw input.
//      This is mainly for mobile.
//      On 12keys-toggle-alphabet mode, a user types "2223" to get "cd".
//      In this case,
//      - Segment's key is "cd".
//      - All the meta candidates are based on "cd" (e.g. "CD", "Cd").
//      Therefore to get "2223" we should access the raw input.
// Prerequisit: |segments| has only one conversion segment.
bool GetNDigits(const composer::Composer &composer,
                const Segments &segments,
                int n,
                string *output) {
  DCHECK(output);
  DCHECK_EQ(1, segments.conversion_segments_size());
  const Segment &segment = segments.conversion_segment(0);

  // 1. Segment's key
  if (IsNDigits(segment.key(), n)) {
    Util::FullWidthAsciiToHalfWidthAscii(segment.key(), output);
    return true;
  }

  // 2. Meta candidates
  for (size_t i = 0; i < segment.meta_candidates_size(); ++i) {
    if (IsNDigits(segment.meta_candidate(i).value, n)) {
      Util::FullWidthAsciiToHalfWidthAscii(segment.meta_candidate(i).value,
                                           output);
      return true;
    }
  }

  // 3. Raw input
  string raw;
  // Note that only one segment is in the Segments, but sometimes like
  // on partial conversion, segment.key() is different from the size of
  // the whole composition.
  composer.GetRawSubString(0, Util::CharsLen(segment.key()), &raw);
  if (IsNDigits(raw, n)) {
    Util::FullWidthAsciiToHalfWidthAscii(raw, output);
    return true;
  }

  // No trials succeeded.
  return false;
}

}  // namespace

bool DateRewriter::RewriteConsecutiveDigits(const composer::Composer &composer,
                                            int insert_position,
                                            Segments *segments) {
  if (segments->conversion_segments_size() != 1) {
    // This method rewrites a segment only when the segments has only
    // one conversion segment.
    // This is spec matter.
    // Rewriting multiple segments will not make users happier.
    return false;
  }
  Segment *segment = segments->mutable_conversion_segment(0);

  // segment->candidate(0) or segment->meta_candidate(0) is used as reference.
  // Check the existence before generating candidates to save time.
  if (segment->candidates_size() == 0 &&
      segment->meta_candidates_size() == 0) {
    VLOG(2) << "No (meta) candidates are found";
    return false;
  }

  // Generate candidates.  The results contain <candidate, description> pairs.
  string number_str;
  std::vector<std::pair<string, const char *>> results;
  if (GetNDigits(composer, *segments, 2, &number_str)) {
    if (!RewriteConsecutiveTwoDigits(number_str, &results)) {
      return false;
    }
  } else if (GetNDigits(composer, *segments, 3, &number_str)) {
    if (!RewriteConsecutiveThreeDigits(number_str, &results)) {
      return false;
    }
  } else if (GetNDigits(composer, *segments, 4, &number_str)) {
    if (!RewriteConsecutiveFourDigits(number_str, &results)) {
      return false;
    }
  }
  if (results.empty()) {
    return false;
  }

  // The existence of segment->candidate(0) or segment->meta_candidate(0) is
  // guaranteed at the above check.
  const Segment::Candidate &top_cand = (segment->candidates_size() > 0)
                                           ? segment->candidate(0)
                                           : segment->meta_candidate(0);
  if (insert_position < 0) {
    insert_position = static_cast<int>(segment->candidates_size());
  }
  for (const auto &result : results) {
    Insert(top_cand, insert_position++, result.first, result.second, segment);
  }

  return true;
}

bool DateRewriter::RewriteConsecutiveTwoDigits(
    StringPiece str,
    std::vector<std::pair<string, const char *>> *results) {
  DCHECK_EQ(2, str.size());
  const auto orig_size = results->size();
  const uint32 high = static_cast<uint32>(str[0] - '0');
  const uint32 low = static_cast<uint32>(str[1] - '0');
  if (IsValidMonthAndDay(high, low)) {
    results->emplace_back(Util::StringPrintf("%c/%c", str[0], str[1]),
                          kDateDescription);
    results->emplace_back(Util::StringPrintf("%c月%c日",
                                             str[0], str[1]),
                          kDateDescription);
  }
  if (IsValidTime(high, low)) {
    // "H時M分".
    GenerateKanjiTimeFormats("%d", "%d", high, low, results);
    // "午前H時M分".
    GenerateGozenGogoTimeFormats("%d", "%d", high, low, results);
  }
  return results->size() > orig_size;
}

bool DateRewriter::RewriteConsecutiveThreeDigits(
    StringPiece str,
    std::vector<std::pair<string, const char *>> *results) {
  DCHECK_EQ(3, str.size());
  const auto orig_size = results->size();

  const uint32 n[] = {static_cast<uint32>(str[0] - '0'),
                      static_cast<uint32>(str[1] - '0'),
                      static_cast<uint32>(str[2] - '0')};

  // Split pattern 1: N|NN
  const uint32 high1 = n[0];
  const uint32 low1 = 10 * n[1] + n[2];
  const bool is_valid_date1 = IsValidMonthAndDay(high1, low1) && str[1] != '0';
  const bool is_valid_time1 = IsValidTime(high1, low1);

  // Split pattern 2: NN|N
  const uint32 high2 = 10 * n[0] + n[1];
  const uint32 low2 = n[2];
  const bool is_valid_date2 = IsValidMonthAndDay(high2, low2) && str[0] != '0';
  const bool is_valid_time2 = IsValidTime(high2, low2) && str[0] != '0';

  if (is_valid_date1) {
    // "M/DD"
    results->emplace_back(Util::StringPrintf("%c/%c%c", str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_date2) {
    // "MM/D"
    results->emplace_back(Util::StringPrintf("%c%c/%c", str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_time1) {
    // "H:MM"
    results->emplace_back(Util::StringPrintf("%c:%c%c", str[0], str[1], str[2]),
                          kTimeDescription);
  }
  // Don't generate HH:M form as it is unusual.

  if (is_valid_date1) {
    // "M月DD日".
    results->emplace_back(Util::StringPrintf("%c月%c%c日",
                                             str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_date2) {
    // "MM月D日"
    results->emplace_back(Util::StringPrintf("%c%c月%c日",
                                             str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_time1) {
    // "M時DD分" etc.
    GenerateKanjiTimeFormats("%d", "%02d", high1, low1, results);
  }
  if (is_valid_time2) {
    // "MM時D分" etc.
    GenerateKanjiTimeFormats("%d", "%d", high2, low2, results);
  }
  if (is_valid_time1) {
    // "午前M時DD分" etc.
    GenerateGozenGogoTimeFormats("%d", "%02d", high1, low1, results);
  }
  if (is_valid_time2) {
    // "午前MM時D分" etc.
    GenerateGozenGogoTimeFormats("%d", "%d", high2, low2, results);
  }

  return results->size() > orig_size;
}

bool DateRewriter::RewriteConsecutiveFourDigits(
    StringPiece str,
    std::vector<std::pair<string, const char *>> *results) {
  DCHECK_EQ(4, str.size());
  const auto orig_size = results->size();

  const uint32 high = (10 * static_cast<uint32>(str[0] - '0') +
                       static_cast<uint32>(str[1] - '0'));
  const uint32 low = (10 * static_cast<uint32>(str[2] - '0') +
                       static_cast<uint32>(str[3] - '0'));

  const bool is_valid_date = IsValidMonthAndDay(high, low);
  const bool is_valid_time = IsValidTime(high, low);

  if (is_valid_date) {
    // "MM/DD"
    results->emplace_back(
        Util::StringPrintf("%c%c/%c%c", str[0], str[1], str[2], str[3]),
        kDateDescription);
  }
  if (is_valid_time) {
    // "MM:DD"
    results->emplace_back(
        Util::StringPrintf("%c%c:%c%c", str[0], str[1], str[2], str[3]),
        kTimeDescription);
  }
  if (is_valid_date && str[0] != '0' && str[2] != '0') {
    // "MM月DD日".  Don't generate this form if there is a leading zero in
    // month or day because it's rarely written like "01月01日".  Don't
    // generate "1月1日" too, as we shouldn't remove the zero explicitly added
    // by user.
    results->emplace_back(
        Util::StringPrintf("%c%c月%c%c日", str[0], str[1],
                           str[2], str[3]),
        kDateDescription);
  }
  if (is_valid_time) {
    // "MM時DD分" etc.
    GenerateKanjiTimeFormats("%02d", "%02d", high, low, results);
    if (high >= 10) {
      // "午前MM時DD分" etc.
      GenerateGozenGogoTimeFormats("%d", "%02d", high, low, results);
    }
  }

  return results->size() > orig_size;
}

DateRewriter::DateRewriter() = default;
DateRewriter::~DateRewriter() = default;

int DateRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool DateRewriter::Rewrite(const ConversionRequest &request,
                           Segments *segments) const {
  if (!request.config().use_date_conversion()) {
    VLOG(2) << "no use_date_conversion";
    return false;
  }

  bool modified = false;

  // Japanese ERA to AD works for resegmented input only
  if (segments->conversion_segments_size() == 1) {
    Segment *seg = segments->mutable_segment(0);
    if (RewriteAd(seg)) {
      return true;
    }
  }

  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    if (seg == nullptr) {
      LOG(ERROR) << "Segment is nullptr";
      return false;
    }

    if (RewriteDate(seg) || RewriteWeekday(seg) ||
        RewriteMonth(seg) || RewriteYear(seg) ||
        RewriteCurrentTime(seg) ||
        RewriteDateAndCurrentTime(seg)) {
      modified = true;
    } else if (i + 1 < segments->segments_size() &&
               RewriteEra(seg, segments->segment(i + 1))) {
      modified = true;
      ++i;  // skip one more
    }
  }

  if (request.has_composer() && segments->conversion_segments_size() > 0) {
    // Select the insert position by Romaji table.  Note:
    // TWELVE_KEYS_TO_HIRAGANA uses digits for Hiragana composing, date/time
    // conversion is performed even when typing Hiragana characters.  Thus, it
    // should not be promoted.
    int insert_pos =
        static_cast<int>(segments->conversion_segment(0).candidates_size());
    switch (request.request().special_romanji_table()) {
      case commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII:
        insert_pos = 1;
        break;
      default:
        break;
    }
    modified |=
        RewriteConsecutiveDigits(request.composer(), insert_pos, segments);
  }

  return modified;
}

}  // namespace mozc
