/*
 *  kanji2.c
 *    sjis, unicode, utf-8のコードマップを出力する 
 *
 *    2013/12/21 V0.10 by oga.
 *    2013/12/24 V0.11 add -name to usage
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#define  TYPE_SJIS  1
#define  TYPE_UNI   2
#define  TYPE_UTF8  3

#define  CODE_MAX1  0x02a6ff    /* デフォではこの辺まで見れればOK */
#define  CODE_MAX2  0x10ffff    /* 全部見たい時(-all)             */

typedef struct _code_area_t {
	int	u_code;
	char *name;
	char *jname;
} code_area_t;

int bomf  = 1;           /* 1: output byte order mark   */
int namef = 0;           /* 1: output charset area name */
int allf  = 0;           /* 1: output all unicode       */
int max   = CODE_MAX1; 

/* see http://ja.wikipedia.org/wiki/Unicode */
const code_area_t code_area[] = {
	{ 0x0000, "Basic Latin", "基本ラテン文字（ASCII互換）"},
	{ 0x0080, "Latin-1 Supplement", "ラテン1補助"},
	{ 0x0100, "Latin Extended-A", "ラテン文字拡張A"},
	{ 0x0180, "Latin Extended-B", "ラテン文字拡張B"},
	{ 0x0250, "IPA Extensions", "IPA拡張（国際音声記号）"},
	{ 0x02B0, "Spacing Modifier Letters", "前進を伴う修飾文字"},
	{ 0x0300, "Combining Diacritical Marks", "ダイアクリティカルマーク（合成可能）"},
	{ 0x0370, "Greek and Coptic", "ギリシア文字及びコプト文字"},
	{ 0x0400, "Cyrillic", "キリール文字（キリル文字）"},
	{ 0x0500, "Cyrillic Supplement", "キリール文字補助"},
	{ 0x0530, "Armenian", "アルメニア文字"},
	{ 0x0590, "Hebrew", "ヘブライ文字"},
	{ 0x0600, "Arabic", "アラビア文字"},
	{ 0x0700, "Syriac", "シリア文字"},
	{ 0x0750, "Arabic Supplement", "アラビア文字補助"},
	{ 0x0780, "Thaana", "ターナ文字"},
	{ 0x07C0, "NKo", "ンコ文字"},
	{ 0x0800, "Samaritan", "サマリア文字 *"},
	{ 0x0840, "Mandaic", "マンダ文字 *"},
	{ 0x08A0, "Arabic Extended-A", "アラビア文字拡張A *"},
	{ 0x0900, "Devanagari", "デーヴァナーガリー文字"},
	{ 0x0980, "Bengali", "ベンガル文字"},
	{ 0x0A00, "Gurmukhi", "グルムキー文字"},
	{ 0x0A80, "Gujarati", "グジャラート文字（グジャラーティー文字）"},
	{ 0x0B00, "Oriya", "オリヤー文字"},
	{ 0x0B80, "Tamil", "タミル文字"},
	{ 0x0C00, "Telugu", "テルグ文字"},
	{ 0x0C80, "Kannada", "カンナダ文字"},
	{ 0x0D00, "Malayalam", "マラヤーラム文字"},
	{ 0x0D80, "Sinhala", "シンハラ文字"},
	{ 0x0E00, "Thai", "タイ文字"},
	{ 0x0E80, "Lao", "ラオス文字（ラーオ文字）"},
	{ 0x0F00, "Tibetan", "チベット文字"},
	{ 0x1000, "Myanmar", "ミャンマー文字（ビルマ文字）"},
	{ 0x10A0, "Georgian", "グルジア文字"},
	{ 0x1100, "Hangul Jamo", "ハングル字母"},
	{ 0x1200, "Ethiopic", "エチオピア文字（ゲエズ文字）"},
	{ 0x1380, "Ethiopic Supplement", "エチオピア文字補助"},
	{ 0x13A0, "Cherokee", "チェロキー文字"},
	{ 0x1400, "Unified Canadian Aboriginal Syllabics", "統合カナダ先住民音節（カナダ先住民文字）"},
	{ 0x1680, "Ogham", "オガム文字"},
	{ 0x16A0, "Runic", "ルーン文字"},
	{ 0x1700, "Tagalog", "タガログ文字（バイバイン）"},
	{ 0x1720, "Hanunoo", "ハヌノオ文字"},
	{ 0x1740, "Buhid", "ブヒッド文字"},
	{ 0x1760, "Tagbanwa", "タグバヌア文字（タグバヌワ文字）"},
	{ 0x1780, "Khmer", "クメール文字"},
	{ 0x1800, "Mongolian", "モンゴル文字"},
	{ 0x18B0, "Unified Canadian Aboriginal Syllabics Extended", "統合カナダ先住民音節拡張 *"},
	{ 0x1900, "Limbu", "リンブ文字"},
	{ 0x1950, "Tai Le", "タイ・ロ文字"},
	{ 0x1980, "New Tai Lue", "新タイ・ロ文字（西双版納タイ文字）"},
	{ 0x19E0, "Khmer Symbols", "クメール文字用記号"},
	{ 0x1A00, "Buginese", "ブギス文字（ロンタラ文字）"},
	{ 0x1A20, "Tai Tham", "ラーンナー文字 *"},
	{ 0x1B00, "Balinese", "バリ文字"},
	{ 0x1B80, "Sundanese", "スンダ文字 *"},
	{ 0x1BC0, "Batak", "バタク文字 *"},
	{ 0x1C00, "Lepcha", "レプチャ文字 *"},
	{ 0x1C50, "Ol Chiki", "オル・チキ文字 *"},
	{ 0x1CC0, "Sundanese Supplement", "スンダ文字補助 *"},
	{ 0x1CD0, "Vedic Extensions", "ヴェーダ文字拡張 *"},
	{ 0x1D00, "Phonetic Extensions", "音声記号拡張"},
	{ 0x1D80, "Phonetic Extensions Supplement", "音声記号拡張補助"},
	{ 0x1DC0, "Combining Diacritical Marks Supplement", "ダイアクリティカルマーク補助（合成可能）補助"},
	{ 0x1E00, "Latin Extended Additional", "ラテン文字拡張追加"},
	{ 0x1F00, "Greek Extended", "ギリシア文字拡張"},
	{ 0x2000, "General Punctuation", "一般句読点"},
	{ 0x2070, "Superscripts and Subscripts", "上付き・下付き"},
	{ 0x20A0, "Currency Symbols", "通貨記号"},
	{ 0x20D0, "Combining Diacritical Marks for Symbols", "記号用ダイアクリティカルマーク（合成可能）"},
	{ 0x2100, "Letterlike Symbols", "文字様記号"},
	{ 0x2150, "Number Forms", "数字に準じるもの"},
	{ 0x2190, "Arrows", "矢印"},
	{ 0x2200, "Mathematical Operators", "数学記号"},
	{ 0x2300, "Miscellaneous Technical", "その他の技術用記号"},
	{ 0x2400, "Control Pictures", "制御機能用記号"},
	{ 0x2440, "Optical Character Recognition", "光学的文字認識、OCR"},
	{ 0x2460, "Enclosed Alphanumerics", "囲み英数字"},
	{ 0x2500, "Box Drawing", "罫線素片"},
	{ 0x2580, "Block Elements", "ブロック要素"},
	{ 0x25A0, "Geometric Shapes", "幾何学模様"},
	{ 0x2600, "Miscellaneous Symbols", "その他の記号"},
	{ 0x2700, "Dingbats", "装飾記号"},
	{ 0x27C0, "Miscellaneous Mathematical Symbols-A", "その他の数学記号A"},
	{ 0x27F0, "Supplemental Arrows-A", "補助矢印A"},
	{ 0x2800, "Braille Patterns", "点字図形"},
	{ 0x2900, "Supplemental Arrows-B", "補助矢印B"},
	{ 0x2980, "Miscellaneous Mathematical Symbols-B", "その他の数学記号B"},
	{ 0x2A00, "Supplemental Mathematical Operators", "補助数学記号"},
	{ 0x2B00, "Miscellaneous Symbols and Arrows", "その他の記号及び矢印"},
	{ 0x2C00, "Glagolitic", "グラゴル文字"},
	{ 0x2C60, "Latin Extended-C", "ラテン文字拡張C"},
	{ 0x2C80, "Coptic", "コプト文字"},
	{ 0x2D00, "Georgian Supplement", "グルジア文字補助"},
	{ 0x2D30, "Tifinagh", "ティフナグ文字"},
	{ 0x2D80, "Ethiopic Extended", "エチオピア文字拡張"},
	{ 0x2DE0, "Cyrillic Extended-A", "キリール文字拡張A *"},
	{ 0x2E00, "Supplemental Punctuation", "補助句読点"},
	{ 0x2E80, "CJK Radicals Supplement", "CJK部首補助"},
	{ 0x2F00, "Kangxi Radicals", "康熙部首"},
	{ 0x2FF0, "Ideographic Description Characters", "漢字構成記述文字、IDC"},
	{ 0x3000, "CJK Symbols and Punctuation", "CJKの記号及び句読点"},
	{ 0x3040, "Hiragana", "平仮名"},
	{ 0x30A0, "Katakana", "片仮名"},
	{ 0x3100, "Bopomofo", "注音字母（注音符号）"},
	{ 0x3130, "Hangul Compatibility Jamo", "ハングル互換字母"},
	{ 0x3190, "Kanbun", "漢文用記号（返り点）"},
	{ 0x31A0, "Bopomofo Extended", "注音字母拡張"},
	{ 0x31C0, "CJK Strokes", "CJKの筆画"},
	{ 0x31F0, "Katakana Phonetic Extensions", "片仮名拡張"},
	{ 0x3200, "Enclosed CJK Letters and Months", "囲みCJK文字・月"},
	{ 0x3300, "CJK Compatibility", "CJK互換用文字"},
	{ 0x3400, "CJK Unified Ideographs Extension A", "CJK統合漢字拡張A"},
	{ 0x4DC0, "Yijing Hexagram Symbols", "易経記号（六十四卦）"},
	{ 0x4E00, "CJK Unified Ideographs", "CJK統合漢字"},
	{ 0xA000, "Yi Syllables", "イ文字（ロロ文字）"},
	{ 0xA490, "Yi Radicals", "イ文字部首"},
	{ 0xA4D0, "Lisu", "リス文字 *"},
	{ 0xA500, "Vai", "ヴァイ文字 *"},
	{ 0xA640, "Cyrillic Extended-B", "キリール文字拡張B *"},
	{ 0xA6A0, "Bamum", "バムン文字 *"},
	{ 0xA700, "Modifier Tone Letters", "声調修飾文字"},
	{ 0xA720, "Latin Extended-D", "ラテン文字拡張D"},
	{ 0xA800, "Syloti Nagri", "シロティ・ナグリ文字"},
	{ 0xA830, "Common Indic Number Forms", "共通インド数字に準じるもの *"},
	{ 0xA840, "Phags-pa", "パスパ文字"},
	{ 0xA880, "Saurashtra", "サウラーシュトラ文字 *"},
	{ 0xA8E0, "Devanagari Extended", "デーヴァナーガリー文字拡張 *"},
	{ 0xA900, "Kayah Li", "カヤー文字 *"},
	{ 0xA930, "Rejang", "レジャン文字 *"},
	{ 0xA960, "Hangul Jamo Extended-A", "ハングル字母拡張A *"},
	{ 0xA980, "Javanese", "ジャワ文字 *"},
	{ 0xAA00, "Cham", "チャム文字 *"},
	{ 0xAA60, "Myanmar Extended-A", "ミャンマー文字拡張A *"},
	{ 0xAA80, "Tai Viet", "タイ・ヴェト文字 *"},
	{ 0xAAE0, "Meetei Mayek Extensions", "マニプリ文字拡張 *"},
	{ 0xAB00, "Ethiopic Extended-A", "エチオピア文字拡張A *"},
	{ 0xABC0, "Meetei Mayek", "マニプリ文字 *"},
	{ 0xAC00, "Hangul Syllables", "ハングル音節文字"},
	{ 0xD7B0, "Hangul Jamo Extended-B", "ハングル字母拡張B *"},
	{ 0xD800, "High Surrogates", "上位代用符号位置"},
	{ 0xDB80, "High Private Use Surrogates", "上位私用代用符号位置"},
	{ 0xDC00, "Low Surrogates", "下位代用符号位置"},
	{ 0xE000, "Private Use Area", "私用領域（外字領域）"},
	{ 0xF900, "CJK Compatibility Ideographs", "CJK互換漢字"},
	{ 0xFB00, "Alphabetic Presentation Forms", "アルファベット表示形"},
	{ 0xFB50, "Arabic Presentation Forms-A", "アラビア表示形A"},
	{ 0xFE00, "Variation Selectors", "字形選択子（異体字セレクタ）"},
	{ 0xFE10, "Vertical Forms", "縦書き形"},
	{ 0xFE20, "Combining Half Marks", "半記号（合成可能）"},
	{ 0xFE30, "CJK Compatibility Forms", "CJK互換形"},
	{ 0xFE50, "Small Form Variants", "小字形"},
	{ 0xFE70, "Arabic Presentation Forms-B", "アラビア表示形B"},
	{ 0xFF00, "Halfwidth and Fullwidth Forms", "半角・全角形"},
	{ 0xFFF0, "Specials", "特殊用途文字"},
	{ 0x10000, "Linear B Syllabary", "線文字B音節文字"},
	{ 0x10080, "Linear B Ideograms", "線文字B表意文字"},
	{ 0x10100, "Aegean Numbers", "エーゲ数字"},
	{ 0x10140, "Ancient Greek Numbers", "古代ギリシア数字"},
	{ 0x10190, "Ancient Symbols", "古代記号 *"},
	{ 0x101D0, "Phaistos Disc", "ファイストスの円盤の文字 *"},
	{ 0x10280, "Lycian", "リキア文字 *"},
	{ 0x102A0, "Carian", "カリア文字 *"},
	{ 0x10300, "Old Italic", "古代イタリア文字（古イタリア文字）"},
	{ 0x10330, "Gothic", "ゴート文字"},
	{ 0x10380, "Ugaritic", "ウガリト文字（ウガリット文字）"},
	{ 0x103A0, "Old Persian", "古代ペルシャ文字"},
	{ 0x10400, "Deseret", "デザレット文字"},
	{ 0x10450, "Shavian", "シェイヴィアン文字"},
	{ 0x10480, "Osmanya", "オスマニア文字"},
	{ 0x10800, "Cypriot Syllabary", "キプロス音節文字"},
	{ 0x10840, "Imperial Aramaic", "アラム文字 *"},
	{ 0x10900, "Phoenician", "フェニキア文字"},
	{ 0x10920, "Lydian", "リディア文字 *"},
	{ 0x10980, "Meroitic Hieroglyphs", "メロエ記念碑書体 *"},
	{ 0x109A0, "Meroitic Cursive", "メロエ草書体 *"},
	{ 0x10A00, "Kharoshthi", "カローシュティー文字"},
	{ 0x10A60, "Old South Arabian", "南アラビア文字 *"},
	{ 0x10B00, "Avestan", "アヴェスター文字 *"},
	{ 0x10B40, "Inscriptional Parthian", "パルティア文字 *"},
	{ 0x10B60, "Inscriptional Pahlavi", "パフラヴィー文字 *"},
	{ 0x10C00, "Old Turkic", "突厥文字 *"},
	{ 0x10E60, "Rumi Numeral Symbols", "ルーミー数字 *"},
	{ 0x11000, "Brahmi", "ブラーフミー文字 *"},
	{ 0x11080, "Kaithi", "カイティ文字 *"},
	{ 0x110D0, "Sora Sompeng", "ソラ・ソンペン文字 *"},
	{ 0x11100, "Chakma", "チャクマ文字 *"},
	{ 0x11180, "Sharada", "シャラダ文字 *"},
	{ 0x11680, "Takri", "タクリ文字 *"},
	{ 0x12000, "Cuneiform", "楔形文字"},
	{ 0x12400, "Cuneiform Numbers and Punctuation", "楔形文字の数字及び句読点"},
	{ 0x13000, "Egyptian Hieroglyphs", "エジプト・ヒエログリフ *"},
	{ 0x16800, "Bamum Supplement", "バムン文字補助 *"},
	{ 0x16F00, "Miao", "ポラード文字 *"},
	{ 0x1B000, "Kana Supplement", "仮名文字補助 *"},
	{ 0x1D000, "Byzantine Musical Symbols", "ビザンチン音楽記号"},
	{ 0x1D100, "Musical Symbols", "音楽記号"},
	{ 0x1D200, "Ancient Greek Musical Notation", "古代ギリシア音符記号"},
	{ 0x1D300, "Tai Xuan Jing Symbols", "太玄経記号"},
	{ 0x1D360, "Counting Rod Numerals", "算木用数字"},
	{ 0x1D400, "Mathematical Alphanumeric Symbols", "数学用英数字記号"},
	{ 0x1EE00, "Arabic Mathematical Alphabetic Symbols", "アラビア数学用文字記号 *"},
	{ 0x1F000, "Mahjong Tiles", "麻雀牌 *"},
	{ 0x1F030, "Domino Tiles", "ドミノ牌 *"},
	{ 0x1F0A0, "Playing Cards", "トランプ *"},
	{ 0x1F100, "Enclosed Alphanumeric Supplement", "囲み英数字補助 *"},
	{ 0x1F200, "Enclosed Ideographic Supplement", "囲み表意文字補助 *"},
	{ 0x1F300, "Miscellaneous Symbols And Pictographs", "その他の記号及び絵文字 *"},
	{ 0x1F600, "Emoticons", "顔文字 *"},
	{ 0x1F680, "Transport And Map Symbols", "交通及び地図の記号 *"},
	{ 0x1F700, "Alchemical Symbols", "錬金術記号 *"},
	{ 0x20000, "CJK Unified Ideographs Extension B", "CJK統合漢字拡張B"},
	{ 0x2A700, "CJK Unified Ideographs Extension C", "CJK統合漢字拡張C *"},
	{ 0x2B740, "CJK Unified Ideographs Extension D", "CJK統合漢字拡張D *"},
	{ 0x2F800, "CJK Compatibility Ideographs Supplement", "CJK互換漢字補助"},
	{ 0xE0000, "Tags", "タグ"},
	{ 0xE0100, "Variation Selectors Supplement", "字形選択子補助"},
	{ 0xF0000, "Supplementary Private Use Area-A", "補助私用領域A *"},
	{ 0x100000, "Supplementary Private Use Area-B", "補助私用領域B *"},
};

void dump_sjis()
{
	int i;
	int high, low;

	printf("code: +0+1+2+3+4+5+6+7+8+9+A+B+C+D+E+F  (SJIS)");
	for (i = 0x0020; i < 0xfd00; i++) {
		if ((0x80 <= i && i <= 0x9f) || (0xe0 <= i && i <= 0xff) ||
			(0x0100 <= i && i < 0x8140) || (0xa000 <= i && i < 0xe03f) ||
			(0x8000 <= i && (i&0xff) < 0x40)) {
			continue;
		}
		if ((i & 0xf) == 0) {
			printf("\n");
			printf("%04x: ", i);
		}
		high = i >> 8;
		low  = i &  0xff;

		if (high == 0) high = 0x20;   /* ASCII area */

		printf("%c%c", high, low);
	}
}

/*
 *  get_area_name()
 *    コードエリアについての名称があれば返す。  
 *
 *    IN  : u_code  Unicode/UTF-8 U+<u_code>
 *    OUT : oname   code area name
 *    OUT : ret     1: name exist  0: no name
 */
int get_area_name(int u_code, char *ostr)
{
	int i;

	strcpy(ostr, "");
	for (i = 0; i < sizeof(code_area)/sizeof(code_area_t); i++) {
		if (u_code == code_area[i].u_code) {
			strcpy(ostr, code_area[i].name);
			return 1;
		}
	}
	return 0;
}

/*
 *  uniprintf()
 *    ascii文字列をUnicodeで出力する。(漢字等は不可)
 *
 */
void uniprintf(char *buf)
{
	int i;
	unsigned short code;
	unsigned short wkcode;

	for (i = 0; i < strlen(buf); i++) {
		code = (unsigned short)buf[i];
		if (code == 0x0a) {
			wkcode = 0x0d;   /* 独自に\n→\r\n展開 */
			fwrite(&wkcode, sizeof(wkcode), 1, stdout);
		}
		fwrite(&code, sizeof(code), 1, stdout);
	}
}


/*
 *  get_uni_str()
 *    U+<x>コードからUnicode(UTF-16)文字列を取り出す。
 *
 *    IN  u_code: Unicodeコード U+<x> の<x>
 *    OUT ocode : 変換後コード(short[2])
 *    OUT ret   : 変換後コード長(shortの個数)
 *
 *    cf.)http://ja.wikipedia.org/wiki/UTF-16
 *    cf.)http://ja.wikipedia.org/wiki/Unicode
 */
int get_uni_str(int u_code, short *ocode)
{
	int  x1, x2, x3, x4;
	int  len = 0;            /* 極力高速化のためstrlen()を使わないことにする */
	int  uu, ww;

	if (u_code <= 0xffff) {
		/* 1short */
		/* U+0000～U+ffff : xxxxxxxx xxxxxxxx(16bit) (0000～FFFF) */
		ocode[0] = u_code;  /* そのまんま */
		len = 1;
	} else {
		/* 4bytes */
		/* U+10000～U+10ffff :             0xd800+xxx       0xdc00+xx
		 * U+000uuuuu xxxxxxxx xxxxxxxx → 110110wwwwxxxxxx 110111xxxxxxxxxx (wwww = uuuuu - 1) 
		 */
		uu = (u_code >> 16);
		ww = uu - 1;
		ww = (ww << 6);
		ocode[0] = 0xd800 + ww + ((u_code & 0xffff) >> 10);
		ocode[1] = 0xdc00 + (u_code & 0x3ff);
		len = 2;
	}
	return len;
}

void dump_uni()
{
	int  i;                   /* U+<i> */
	int  j;
	int  len;
	short wkcode;
	short code[2];
	char buf[2048];
	char wk[2048];

#ifdef _WIN32
	setmode(fileno(stdout), O_BINARY);   /* fwrite()での \n の展開を抑止 */
#endif /* _WIN32 */

	if (bomf) {
		wkcode = 0xfeff;
		fwrite(&wkcode, sizeof(wkcode), 1, stdout);  /* This OS's byte order mark */
	}

	uniprintf("U+xxxxxx (realcode): +0+1+2+3+4+5+6+7+8+9+A+B+C+D+E+F  (Unicode)");
	/* for (i = 0x0020; i < 0x02a700; i++)  */  /* U+<i> UTF-8 Win7(JIS2004)ではここあたりがMAX */
	for (i = 0x0020; i <= max; i++)
	{
		/* 0xd800-0xdb7f: High Surrogates             */
		/* 0xdb80-0xdbff: High Private Use Surrogates */
		/* 0xdc00-0xdfff: Low Surrogates              */
		/* 0xe000-0xf8ff: Private Use Area(Gaiji Area)*/
		if (allf == 0) {
			if ((0xd800 <= i && i < 0xe000) || (0xe900 <= i && i < 0xf900) ||
				(0x10000 <= i && i < 0x20000)) {
				/* 表示スキップ */
				continue;
			}
		}

		if ((i & 0xf) == 0) {
			uniprintf("\n");
			if (namef && get_area_name(i, buf)) {
				/* print area name */
				sprintf(wk, "--- %s ---\n", buf);
				uniprintf(wk);
			}
			len = get_uni_str(i, code);
			/* 実コードのHex文字列へ変換 */
			for (j = 0; j < len; j++) {
				sprintf(&wk[j*4], "%04x", (unsigned short)code[j]);
			}
			wk[len*4] = '\0';
			sprintf(buf, "U+%06x (%8s): ", i, wk);
			uniprintf(buf);
		}

		len = get_uni_str(i, code);
		if (i < 0x100) {
			/* ascii */
			wkcode = 0x20;   /* space */
			fwrite(&wkcode, sizeof(short), 1, stdout);   /* put space */
			fwrite(&code[0], sizeof(short), 1, stdout);  /* put char  */
		} else {
			/* 2,4byte */
			for (j = 0; j < len; j++) {
				fwrite(&code[j], sizeof(short), 1, stdout);
			}
		}
	}
}

/*
 *  get_utf8_str()
 *    U+<x>コードからUTF-8文字列を取り出す。
 *
 *    IN  u_code: UTF-8コード U+<x> の<x>
 *    OUT ostr  : 変換後文字列 
 *    OUT ret   : 文字列長  
 *
 *    cf.)http://gihyo.jp/admin/serial/01/charcode/0004
 *    cf.)http://ja.wikipedia.org/wiki/UTF-8
 *    cf.)http://www.softel.co.jp/blogs/tech/archives/596
 *
 */
int get_utf8_str(int u_code, char *ostr)
{
	int  x1, x2, x3, x4;
	int  len = 0;            /* 極力高速化のためstrlen()を使わないことにする */

	if (u_code <= 0x7f) {
		/* 1byte  */
		/* U+0000～U+007f : 0xxxxxxx (00～7F) */
		sprintf(ostr, "%c", u_code);
		len = 1;
	} else if (u_code <= 0x7ff) {
		/* 2bytes */
		/* U+0080～U+07ff : 110xxxxx(C2～DF) 10xxxxxx(80～BF)  */
		x1 = 0xc0 + (u_code >> 6);
		x2 = 0x80 + (u_code & 0x3f);
		sprintf(ostr, "%c%c", x1, x2);
		len = 2;
	} else if (u_code <= 0xffff) {
		/* 3bytes */
		/* U+0800～U+ffff : 1110xxxx(E0～EF) 10xxxxxx(80～BF) 10xxxxxx(80～BF) */
		x1 = 0xe0 + (u_code >> 12);
		x2 = 0x80 + ((u_code & 0xfff) >> 6);
		x3 = 0x80 + (u_code & 0x3f);
		sprintf(ostr, "%c%c%c", x1, x2, x3);
		len = 3;
	} else {
		/* 4bytes */
		/* U+10000～U+10ffff : 11110xxx(F0～F7) 10xxxxxx(80～BF) 10xxxxxx(80～BF) 10xxxxxx(80～BF) */
		x1 = 0xf0 + (u_code >> 18);
		x2 = 0x80 + ((u_code & 0x3ffff) >> 12);
		x3 = 0x80 + ((u_code & 0xfff) >> 6);
		x4 = 0x80 + (u_code & 0x3f);
		sprintf(ostr, "%c%c%c%c", x1, x2, x3, x4);
		len = 4;
	}
	return len;
}

void dump_utf8()
{
	int  i;                   /* U+<i> */
	int  j;
	int  len;
	char buf[2048];
	char wk[2048];

	if (bomf) {
		printf("%c%c%c", 0xef, 0xbb, 0xbf);  /* = get_utf8_str(0xfeff, buf) */
	}

	printf("U+xxxxxx (realcode): +0+1+2+3+4+5+6+7+8+9+A+B+C+D+E+F  (UTF-8)");
	for (i = 0x0020; i <= max; i++)   /* U+<i> Win7(JIS2004)ではここあたりがMAX */
	{
		if (allf == 0) {
			if ((0xd800 <= i && i < 0xe000) || (0xe900 <= i && i < 0xf900) ||
				(0x10000 <= i && i < 0x20000)) {
				/* 表示スキップ */
				continue;
			}
		}

		if ((i & 0xf) == 0) {
			printf("\n");
			if (namef && get_area_name(i, buf)) {
				/* print area name */
				printf("--- %s ---\n", buf);
			}

			len = get_utf8_str(i, buf);
			/* 実コードのHex文字列へ変換 */
			for (j = 0; j < len; j++) {
				sprintf(&wk[j*2], "%02x", (unsigned char)buf[j]);
			}
			wk[len*2] = '\0';
			printf("U+%06x (%8s): ", i, wk);
		}

		len = get_utf8_str(i, buf);

		/* if (len == 1) */
		if (i < 0x100) {
			printf(" %s", buf);   /* 1byte */
		} else {
			printf("%s", buf);    /* 2-4byte */
		}
	}
}

int main(int a, char *b[])
{
	int i;
	int type;

	type = TYPE_SJIS;   /* default */

	for (i = 1; i<a; i++) {
		if (!strcmp(b[i], "-h")) {
			printf("usage: kanji2 [{<-sjis>|-uni|-utf8}] [-nobom] [-all] [-name]\n");
			printf("  -all   : Output 0x20-0x%06x\n", CODE_MAX2);
			printf("  -name  : Outut area name.\n");
			printf("  -nobom : Don't output byte order mark.\n");
			return 1;
		}
		if (!strcmp(b[i], "-sjis")) {
			type = TYPE_SJIS;
			continue;
		}
		if (!strcmp(b[i], "-uni")) {
			type = TYPE_UNI;
			continue;
		}
		if (!strcmp(b[i], "-utf8")) {
			type = TYPE_UTF8;
			continue;
		}
		if (!strcmp(b[i], "-name")) {
			namef = 1;
			continue;
		}
		if (!strcmp(b[i], "-all")) {
			allf = 1;
			max  = CODE_MAX2;
			continue;
		}
		if (!strcmp(b[i], "-nobom")) {
			bomf = 0;
			continue;
		}
	}

	switch (type) {
	  case TYPE_SJIS:
	    dump_sjis();
	    break;
	  case TYPE_UNI:
	    dump_uni();
	    break;
	  case TYPE_UTF8:
	    dump_utf8();
	    break;
	  default:
	  	printf("invalid type.\n");
	    break;
	}

	return 0;
}

/* vim:ts=4:sw=4:
 */

