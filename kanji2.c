/*
 *  kanji2.c
 *    sjis, unicode, utf-8�̃R�[�h�}�b�v���o�͂��� 
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

#define  CODE_MAX1  0x02a6ff    /* �f�t�H�ł͂��̕ӂ܂Ō�����OK */
#define  CODE_MAX2  0x10ffff    /* �S����������(-all)             */

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
	{ 0x0000, "Basic Latin", "��{���e�������iASCII�݊��j"},
	{ 0x0080, "Latin-1 Supplement", "���e��1�⏕"},
	{ 0x0100, "Latin Extended-A", "���e�������g��A"},
	{ 0x0180, "Latin Extended-B", "���e�������g��B"},
	{ 0x0250, "IPA Extensions", "IPA�g���i���ۉ����L���j"},
	{ 0x02B0, "Spacing Modifier Letters", "�O�i�𔺂��C������"},
	{ 0x0300, "Combining Diacritical Marks", "�_�C�A�N���e�B�J���}�[�N�i�����\�j"},
	{ 0x0370, "Greek and Coptic", "�M���V�A�����y�уR�v�g����"},
	{ 0x0400, "Cyrillic", "�L���[�������i�L���������j"},
	{ 0x0500, "Cyrillic Supplement", "�L���[�������⏕"},
	{ 0x0530, "Armenian", "�A�����j�A����"},
	{ 0x0590, "Hebrew", "�w�u���C����"},
	{ 0x0600, "Arabic", "�A���r�A����"},
	{ 0x0700, "Syriac", "�V���A����"},
	{ 0x0750, "Arabic Supplement", "�A���r�A�����⏕"},
	{ 0x0780, "Thaana", "�^�[�i����"},
	{ 0x07C0, "NKo", "���R����"},
	{ 0x0800, "Samaritan", "�T�}���A���� *"},
	{ 0x0840, "Mandaic", "�}���_���� *"},
	{ 0x08A0, "Arabic Extended-A", "�A���r�A�����g��A *"},
	{ 0x0900, "Devanagari", "�f�[���@�i�[�K���[����"},
	{ 0x0980, "Bengali", "�x���K������"},
	{ 0x0A00, "Gurmukhi", "�O�����L�[����"},
	{ 0x0A80, "Gujarati", "�O�W�����[�g�����i�O�W�����[�e�B�[�����j"},
	{ 0x0B00, "Oriya", "�I�����[����"},
	{ 0x0B80, "Tamil", "�^�~������"},
	{ 0x0C00, "Telugu", "�e���O����"},
	{ 0x0C80, "Kannada", "�J���i�_����"},
	{ 0x0D00, "Malayalam", "�}�����[��������"},
	{ 0x0D80, "Sinhala", "�V���n������"},
	{ 0x0E00, "Thai", "�^�C����"},
	{ 0x0E80, "Lao", "���I�X�����i���[�I�����j"},
	{ 0x0F00, "Tibetan", "�`�x�b�g����"},
	{ 0x1000, "Myanmar", "�~�����}�[�����i�r���}�����j"},
	{ 0x10A0, "Georgian", "�O���W�A����"},
	{ 0x1100, "Hangul Jamo", "�n���O������"},
	{ 0x1200, "Ethiopic", "�G�`�I�s�A�����i�Q�G�Y�����j"},
	{ 0x1380, "Ethiopic Supplement", "�G�`�I�s�A�����⏕"},
	{ 0x13A0, "Cherokee", "�`�F���L�[����"},
	{ 0x1400, "Unified Canadian Aboriginal Syllabics", "�����J�i�_��Z�����߁i�J�i�_��Z�������j"},
	{ 0x1680, "Ogham", "�I�K������"},
	{ 0x16A0, "Runic", "���[������"},
	{ 0x1700, "Tagalog", "�^�K���O�����i�o�C�o�C���j"},
	{ 0x1720, "Hanunoo", "�n�k�m�I����"},
	{ 0x1740, "Buhid", "�u�q�b�h����"},
	{ 0x1760, "Tagbanwa", "�^�O�o�k�A�����i�^�O�o�k�������j"},
	{ 0x1780, "Khmer", "�N���[������"},
	{ 0x1800, "Mongolian", "�����S������"},
	{ 0x18B0, "Unified Canadian Aboriginal Syllabics Extended", "�����J�i�_��Z�����ߊg�� *"},
	{ 0x1900, "Limbu", "�����u����"},
	{ 0x1950, "Tai Le", "�^�C�E������"},
	{ 0x1980, "New Tai Lue", "�V�^�C�E�������i���o�Ŕ[�^�C�����j"},
	{ 0x19E0, "Khmer Symbols", "�N���[�������p�L��"},
	{ 0x1A00, "Buginese", "�u�M�X�����i�����^�������j"},
	{ 0x1A20, "Tai Tham", "���[���i�[���� *"},
	{ 0x1B00, "Balinese", "�o������"},
	{ 0x1B80, "Sundanese", "�X���_���� *"},
	{ 0x1BC0, "Batak", "�o�^�N���� *"},
	{ 0x1C00, "Lepcha", "���v�`������ *"},
	{ 0x1C50, "Ol Chiki", "�I���E�`�L���� *"},
	{ 0x1CC0, "Sundanese Supplement", "�X���_�����⏕ *"},
	{ 0x1CD0, "Vedic Extensions", "���F�[�_�����g�� *"},
	{ 0x1D00, "Phonetic Extensions", "�����L���g��"},
	{ 0x1D80, "Phonetic Extensions Supplement", "�����L���g���⏕"},
	{ 0x1DC0, "Combining Diacritical Marks Supplement", "�_�C�A�N���e�B�J���}�[�N�⏕�i�����\�j�⏕"},
	{ 0x1E00, "Latin Extended Additional", "���e�������g���ǉ�"},
	{ 0x1F00, "Greek Extended", "�M���V�A�����g��"},
	{ 0x2000, "General Punctuation", "��ʋ�Ǔ_"},
	{ 0x2070, "Superscripts and Subscripts", "��t���E���t��"},
	{ 0x20A0, "Currency Symbols", "�ʉ݋L��"},
	{ 0x20D0, "Combining Diacritical Marks for Symbols", "�L���p�_�C�A�N���e�B�J���}�[�N�i�����\�j"},
	{ 0x2100, "Letterlike Symbols", "�����l�L��"},
	{ 0x2150, "Number Forms", "�����ɏ��������"},
	{ 0x2190, "Arrows", "���"},
	{ 0x2200, "Mathematical Operators", "���w�L��"},
	{ 0x2300, "Miscellaneous Technical", "���̑��̋Z�p�p�L��"},
	{ 0x2400, "Control Pictures", "����@�\�p�L��"},
	{ 0x2440, "Optical Character Recognition", "���w�I�����F���AOCR"},
	{ 0x2460, "Enclosed Alphanumerics", "�͂݉p����"},
	{ 0x2500, "Box Drawing", "�r���f��"},
	{ 0x2580, "Block Elements", "�u���b�N�v�f"},
	{ 0x25A0, "Geometric Shapes", "�􉽊w�͗l"},
	{ 0x2600, "Miscellaneous Symbols", "���̑��̋L��"},
	{ 0x2700, "Dingbats", "�����L��"},
	{ 0x27C0, "Miscellaneous Mathematical Symbols-A", "���̑��̐��w�L��A"},
	{ 0x27F0, "Supplemental Arrows-A", "�⏕���A"},
	{ 0x2800, "Braille Patterns", "�_���}�`"},
	{ 0x2900, "Supplemental Arrows-B", "�⏕���B"},
	{ 0x2980, "Miscellaneous Mathematical Symbols-B", "���̑��̐��w�L��B"},
	{ 0x2A00, "Supplemental Mathematical Operators", "�⏕���w�L��"},
	{ 0x2B00, "Miscellaneous Symbols and Arrows", "���̑��̋L���y�і��"},
	{ 0x2C00, "Glagolitic", "�O���S������"},
	{ 0x2C60, "Latin Extended-C", "���e�������g��C"},
	{ 0x2C80, "Coptic", "�R�v�g����"},
	{ 0x2D00, "Georgian Supplement", "�O���W�A�����⏕"},
	{ 0x2D30, "Tifinagh", "�e�B�t�i�O����"},
	{ 0x2D80, "Ethiopic Extended", "�G�`�I�s�A�����g��"},
	{ 0x2DE0, "Cyrillic Extended-A", "�L���[�������g��A *"},
	{ 0x2E00, "Supplemental Punctuation", "�⏕��Ǔ_"},
	{ 0x2E80, "CJK Radicals Supplement", "CJK����⏕"},
	{ 0x2F00, "Kangxi Radicals", "�Nꤕ���"},
	{ 0x2FF0, "Ideographic Description Characters", "�����\���L�q�����AIDC"},
	{ 0x3000, "CJK Symbols and Punctuation", "CJK�̋L���y�ы�Ǔ_"},
	{ 0x3040, "Hiragana", "������"},
	{ 0x30A0, "Katakana", "�Љ���"},
	{ 0x3100, "Bopomofo", "��������i���������j"},
	{ 0x3130, "Hangul Compatibility Jamo", "�n���O���݊�����"},
	{ 0x3190, "Kanbun", "�����p�L���i�Ԃ�_�j"},
	{ 0x31A0, "Bopomofo Extended", "��������g��"},
	{ 0x31C0, "CJK Strokes", "CJK�̕M��"},
	{ 0x31F0, "Katakana Phonetic Extensions", "�Љ����g��"},
	{ 0x3200, "Enclosed CJK Letters and Months", "�͂�CJK�����E��"},
	{ 0x3300, "CJK Compatibility", "CJK�݊��p����"},
	{ 0x3400, "CJK Unified Ideographs Extension A", "CJK���������g��A"},
	{ 0x4DC0, "Yijing Hexagram Symbols", "�Ռo�L���i�Z�\�l�T�j"},
	{ 0x4E00, "CJK Unified Ideographs", "CJK��������"},
	{ 0xA000, "Yi Syllables", "�C�����i���������j"},
	{ 0xA490, "Yi Radicals", "�C��������"},
	{ 0xA4D0, "Lisu", "���X���� *"},
	{ 0xA500, "Vai", "���@�C���� *"},
	{ 0xA640, "Cyrillic Extended-B", "�L���[�������g��B *"},
	{ 0xA6A0, "Bamum", "�o�������� *"},
	{ 0xA700, "Modifier Tone Letters", "�����C������"},
	{ 0xA720, "Latin Extended-D", "���e�������g��D"},
	{ 0xA800, "Syloti Nagri", "�V���e�B�E�i�O������"},
	{ 0xA830, "Common Indic Number Forms", "���ʃC���h�����ɏ�������� *"},
	{ 0xA840, "Phags-pa", "�p�X�p����"},
	{ 0xA880, "Saurashtra", "�T�E���[�V���g������ *"},
	{ 0xA8E0, "Devanagari Extended", "�f�[���@�i�[�K���[�����g�� *"},
	{ 0xA900, "Kayah Li", "�J���[���� *"},
	{ 0xA930, "Rejang", "���W�������� *"},
	{ 0xA960, "Hangul Jamo Extended-A", "�n���O������g��A *"},
	{ 0xA980, "Javanese", "�W�������� *"},
	{ 0xAA00, "Cham", "�`�������� *"},
	{ 0xAA60, "Myanmar Extended-A", "�~�����}�[�����g��A *"},
	{ 0xAA80, "Tai Viet", "�^�C�E���F�g���� *"},
	{ 0xAAE0, "Meetei Mayek Extensions", "�}�j�v�������g�� *"},
	{ 0xAB00, "Ethiopic Extended-A", "�G�`�I�s�A�����g��A *"},
	{ 0xABC0, "Meetei Mayek", "�}�j�v������ *"},
	{ 0xAC00, "Hangul Syllables", "�n���O�����ߕ���"},
	{ 0xD7B0, "Hangul Jamo Extended-B", "�n���O������g��B *"},
	{ 0xD800, "High Surrogates", "��ʑ�p�����ʒu"},
	{ 0xDB80, "High Private Use Surrogates", "��ʎ��p��p�����ʒu"},
	{ 0xDC00, "Low Surrogates", "���ʑ�p�����ʒu"},
	{ 0xE000, "Private Use Area", "���p�̈�i�O���̈�j"},
	{ 0xF900, "CJK Compatibility Ideographs", "CJK�݊�����"},
	{ 0xFB00, "Alphabetic Presentation Forms", "�A���t�@�x�b�g�\���`"},
	{ 0xFB50, "Arabic Presentation Forms-A", "�A���r�A�\���`A"},
	{ 0xFE00, "Variation Selectors", "���`�I���q�i�ّ̎��Z���N�^�j"},
	{ 0xFE10, "Vertical Forms", "�c�����`"},
	{ 0xFE20, "Combining Half Marks", "���L���i�����\�j"},
	{ 0xFE30, "CJK Compatibility Forms", "CJK�݊��`"},
	{ 0xFE50, "Small Form Variants", "�����`"},
	{ 0xFE70, "Arabic Presentation Forms-B", "�A���r�A�\���`B"},
	{ 0xFF00, "Halfwidth and Fullwidth Forms", "���p�E�S�p�`"},
	{ 0xFFF0, "Specials", "����p�r����"},
	{ 0x10000, "Linear B Syllabary", "������B���ߕ���"},
	{ 0x10080, "Linear B Ideograms", "������B�\�ӕ���"},
	{ 0x10100, "Aegean Numbers", "�G�[�Q����"},
	{ 0x10140, "Ancient Greek Numbers", "�Ñ�M���V�A����"},
	{ 0x10190, "Ancient Symbols", "�Ñ�L�� *"},
	{ 0x101D0, "Phaistos Disc", "�t�@�C�X�g�X�̉~�Ղ̕��� *"},
	{ 0x10280, "Lycian", "���L�A���� *"},
	{ 0x102A0, "Carian", "�J���A���� *"},
	{ 0x10300, "Old Italic", "�Ñ�C�^���A�����i�ÃC�^���A�����j"},
	{ 0x10330, "Gothic", "�S�[�g����"},
	{ 0x10380, "Ugaritic", "�E�K���g�����i�E�K���b�g�����j"},
	{ 0x103A0, "Old Persian", "�Ñ�y���V������"},
	{ 0x10400, "Deseret", "�f�U���b�g����"},
	{ 0x10450, "Shavian", "�V�F�C���B�A������"},
	{ 0x10480, "Osmanya", "�I�X�}�j�A����"},
	{ 0x10800, "Cypriot Syllabary", "�L�v���X���ߕ���"},
	{ 0x10840, "Imperial Aramaic", "�A�������� *"},
	{ 0x10900, "Phoenician", "�t�F�j�L�A����"},
	{ 0x10920, "Lydian", "���f�B�A���� *"},
	{ 0x10980, "Meroitic Hieroglyphs", "�����G�L�O�菑�� *"},
	{ 0x109A0, "Meroitic Cursive", "�����G������ *"},
	{ 0x10A00, "Kharoshthi", "�J���[�V���e�B�[����"},
	{ 0x10A60, "Old South Arabian", "��A���r�A���� *"},
	{ 0x10B00, "Avestan", "�A���F�X�^�[���� *"},
	{ 0x10B40, "Inscriptional Parthian", "�p���e�B�A���� *"},
	{ 0x10B60, "Inscriptional Pahlavi", "�p�t�����B�[���� *"},
	{ 0x10C00, "Old Turkic", "�˙Ε��� *"},
	{ 0x10E60, "Rumi Numeral Symbols", "���[�~�[���� *"},
	{ 0x11000, "Brahmi", "�u���[�t�~�[���� *"},
	{ 0x11080, "Kaithi", "�J�C�e�B���� *"},
	{ 0x110D0, "Sora Sompeng", "�\���E�\���y������ *"},
	{ 0x11100, "Chakma", "�`���N�}���� *"},
	{ 0x11180, "Sharada", "�V�����_���� *"},
	{ 0x11680, "Takri", "�^�N������ *"},
	{ 0x12000, "Cuneiform", "���`����"},
	{ 0x12400, "Cuneiform Numbers and Punctuation", "���`�����̐����y�ы�Ǔ_"},
	{ 0x13000, "Egyptian Hieroglyphs", "�G�W�v�g�E�q�G���O���t *"},
	{ 0x16800, "Bamum Supplement", "�o���������⏕ *"},
	{ 0x16F00, "Miao", "�|���[�h���� *"},
	{ 0x1B000, "Kana Supplement", "���������⏕ *"},
	{ 0x1D000, "Byzantine Musical Symbols", "�r�U���`�����y�L��"},
	{ 0x1D100, "Musical Symbols", "���y�L��"},
	{ 0x1D200, "Ancient Greek Musical Notation", "�Ñ�M���V�A�����L��"},
	{ 0x1D300, "Tai Xuan Jing Symbols", "�����o�L��"},
	{ 0x1D360, "Counting Rod Numerals", "�Z�ؗp����"},
	{ 0x1D400, "Mathematical Alphanumeric Symbols", "���w�p�p�����L��"},
	{ 0x1EE00, "Arabic Mathematical Alphabetic Symbols", "�A���r�A���w�p�����L�� *"},
	{ 0x1F000, "Mahjong Tiles", "�����v *"},
	{ 0x1F030, "Domino Tiles", "�h�~�m�v *"},
	{ 0x1F0A0, "Playing Cards", "�g�����v *"},
	{ 0x1F100, "Enclosed Alphanumeric Supplement", "�͂݉p�����⏕ *"},
	{ 0x1F200, "Enclosed Ideographic Supplement", "�͂ݕ\�ӕ����⏕ *"},
	{ 0x1F300, "Miscellaneous Symbols And Pictographs", "���̑��̋L���y�ъG���� *"},
	{ 0x1F600, "Emoticons", "�當�� *"},
	{ 0x1F680, "Transport And Map Symbols", "��ʋy�ђn�}�̋L�� *"},
	{ 0x1F700, "Alchemical Symbols", "�B���p�L�� *"},
	{ 0x20000, "CJK Unified Ideographs Extension B", "CJK���������g��B"},
	{ 0x2A700, "CJK Unified Ideographs Extension C", "CJK���������g��C *"},
	{ 0x2B740, "CJK Unified Ideographs Extension D", "CJK���������g��D *"},
	{ 0x2F800, "CJK Compatibility Ideographs Supplement", "CJK�݊������⏕"},
	{ 0xE0000, "Tags", "�^�O"},
	{ 0xE0100, "Variation Selectors Supplement", "���`�I���q�⏕"},
	{ 0xF0000, "Supplementary Private Use Area-A", "�⏕���p�̈�A *"},
	{ 0x100000, "Supplementary Private Use Area-B", "�⏕���p�̈�B *"},
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
 *    �R�[�h�G���A�ɂ��Ă̖��̂�����ΕԂ��B  
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
 *    ascii�������Unicode�ŏo�͂���B(�������͕s��)
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
			wkcode = 0x0d;   /* �Ǝ���\n��\r\n�W�J */
			fwrite(&wkcode, sizeof(wkcode), 1, stdout);
		}
		fwrite(&code, sizeof(code), 1, stdout);
	}
}


/*
 *  get_uni_str()
 *    U+<x>�R�[�h����Unicode(UTF-16)����������o���B
 *
 *    IN  u_code: Unicode�R�[�h U+<x> ��<x>
 *    OUT ocode : �ϊ���R�[�h(short[2])
 *    OUT ret   : �ϊ���R�[�h��(short�̌�)
 *
 *    cf.)http://ja.wikipedia.org/wiki/UTF-16
 *    cf.)http://ja.wikipedia.org/wiki/Unicode
 */
int get_uni_str(int u_code, short *ocode)
{
	int  x1, x2, x3, x4;
	int  len = 0;            /* �ɗ͍������̂���strlen()���g��Ȃ����Ƃɂ��� */
	int  uu, ww;

	if (u_code <= 0xffff) {
		/* 1short */
		/* U+0000�`U+ffff : xxxxxxxx xxxxxxxx(16bit) (0000�`FFFF) */
		ocode[0] = u_code;  /* ���̂܂�� */
		len = 1;
	} else {
		/* 4bytes */
		/* U+10000�`U+10ffff :             0xd800+xxx       0xdc00+xx
		 * U+000uuuuu xxxxxxxx xxxxxxxx �� 110110wwwwxxxxxx 110111xxxxxxxxxx (wwww = uuuuu - 1) 
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
	setmode(fileno(stdout), O_BINARY);   /* fwrite()�ł� \n �̓W�J��}�~ */
#endif /* _WIN32 */

	if (bomf) {
		wkcode = 0xfeff;
		fwrite(&wkcode, sizeof(wkcode), 1, stdout);  /* This OS's byte order mark */
	}

	uniprintf("U+xxxxxx (realcode): +0+1+2+3+4+5+6+7+8+9+A+B+C+D+E+F  (Unicode)");
	/* for (i = 0x0020; i < 0x02a700; i++)  */  /* U+<i> UTF-8 Win7(JIS2004)�ł͂��������肪MAX */
	for (i = 0x0020; i <= max; i++)
	{
		/* 0xd800-0xdb7f: High Surrogates             */
		/* 0xdb80-0xdbff: High Private Use Surrogates */
		/* 0xdc00-0xdfff: Low Surrogates              */
		/* 0xe000-0xf8ff: Private Use Area(Gaiji Area)*/
		if (allf == 0) {
			if ((0xd800 <= i && i < 0xe000) || (0xe900 <= i && i < 0xf900) ||
				(0x10000 <= i && i < 0x20000)) {
				/* �\���X�L�b�v */
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
			/* ���R�[�h��Hex������֕ϊ� */
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
 *    U+<x>�R�[�h����UTF-8����������o���B
 *
 *    IN  u_code: UTF-8�R�[�h U+<x> ��<x>
 *    OUT ostr  : �ϊ��㕶���� 
 *    OUT ret   : ������  
 *
 *    cf.)http://gihyo.jp/admin/serial/01/charcode/0004
 *    cf.)http://ja.wikipedia.org/wiki/UTF-8
 *    cf.)http://www.softel.co.jp/blogs/tech/archives/596
 *
 */
int get_utf8_str(int u_code, char *ostr)
{
	int  x1, x2, x3, x4;
	int  len = 0;            /* �ɗ͍������̂���strlen()���g��Ȃ����Ƃɂ��� */

	if (u_code <= 0x7f) {
		/* 1byte  */
		/* U+0000�`U+007f : 0xxxxxxx (00�`7F) */
		sprintf(ostr, "%c", u_code);
		len = 1;
	} else if (u_code <= 0x7ff) {
		/* 2bytes */
		/* U+0080�`U+07ff : 110xxxxx(C2�`DF) 10xxxxxx(80�`BF)  */
		x1 = 0xc0 + (u_code >> 6);
		x2 = 0x80 + (u_code & 0x3f);
		sprintf(ostr, "%c%c", x1, x2);
		len = 2;
	} else if (u_code <= 0xffff) {
		/* 3bytes */
		/* U+0800�`U+ffff : 1110xxxx(E0�`EF) 10xxxxxx(80�`BF) 10xxxxxx(80�`BF) */
		x1 = 0xe0 + (u_code >> 12);
		x2 = 0x80 + ((u_code & 0xfff) >> 6);
		x3 = 0x80 + (u_code & 0x3f);
		sprintf(ostr, "%c%c%c", x1, x2, x3);
		len = 3;
	} else {
		/* 4bytes */
		/* U+10000�`U+10ffff : 11110xxx(F0�`F7) 10xxxxxx(80�`BF) 10xxxxxx(80�`BF) 10xxxxxx(80�`BF) */
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
	for (i = 0x0020; i <= max; i++)   /* U+<i> Win7(JIS2004)�ł͂��������肪MAX */
	{
		if (allf == 0) {
			if ((0xd800 <= i && i < 0xe000) || (0xe900 <= i && i < 0xf900) ||
				(0x10000 <= i && i < 0x20000)) {
				/* �\���X�L�b�v */
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
			/* ���R�[�h��Hex������֕ϊ� */
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

