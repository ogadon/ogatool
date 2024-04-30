/*
 *   ID3 Tag disp
 *
 *   00/08/11 V0.10  by oga
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TAG_SIZE 128       /* ID3 V1.0-1.1 tag size */

char *GenreTbl[] =
{
    "Blues",
    "Classic Rock",
    "Country",
    "Dance",
    "Disco",
    "Funk",
    "Grunge",
    "Hip-Hop",
    "Jazz",
    "Metal",
    "New Age",
    "Oldies",
    "Other",
    "Pop",
    "R&B",
    "Rap",
    "Reggae",
    "Rock",
    "Techno",
    "Industrial",
    "Alternative",
    "Ska",
    "Death Metal",
    "Pranks",
    "Soundtrack",
    "Euro-Techno",
    "Ambient",
    "Trip-Hop",
    "Vocal",
    "Jazz+Funk",
    "Fusion",
    "Trance",
    "Classical",
    "Instrumental",
    "Acid",
    "House",
    "Game",
    "Sound Clip",
    "Gospel",
    "Noise",
    "AlternRock",
    "Bass",
    "Soul",
    "Punk",
    "Space",
    "Meditative",
    "Instrumental Pop",
    "Instrumental Rock",
    "Ethnic",
    "Gothic",
    "Darkwave",
    "Techno-Industrial",
    "Electronic",
    "Pop-Folk",
    "Eurodance",
    "Dream",
    "Southern Rock",
    "Comedy",
    "Cult",
    "Gangsta",
    "Top 40",
    "Christian Rap",
    "Pop/Funk",
    "Jungle",
    "Native American",
    "Cabaret",
    "New Wave",
    "Psychadelic",
    "Rave",
    "Showtunes",
    "Trailer",
    "Lo-Fi",
    "Tribal",
    "Acid Punk",
    "Acid Jazz",
    "Polka",
    "Retro",
    "Musical",
    "Rock & Roll",
    "Hard Rock",
    "Folk",
    "Folk/Rock",
    "National folk",
    "Swing",
    "Fast-fusion",
    "Bebob",
    "Latin",
    "Revival",
    "Celtic",
    "Bluegrass",
    "Avantgarde",
    "Gothic Rock",
    "Progressive Rock",
    "Psychedelic Rock",
    "Symphonic Rock",
    "Slow Rock",
    "Big Band",
    "Chorus",
    "Easy Listening",
    "Acoustic",
    "Humour",
    "Speech",
    "Chanson",
    "Opera",
    "Chamber Music",
    "Sonata",
    "Symphony",
    "Booty Bass",
    "Primus",
    "Porn Groove",
    "Satire",
    "Slow Jam",
    "Club",
    "Tango",
    "Samba",
    "Folklore",
    "Ballad",
    "Powder Ballad",
    "Rhythmic Soul",
    "Freestyle",
    "Duet",
    "Punk Rock",
    "Drum Solo",
    "A Capella",
    "Euro-House",
    "Dance Hall",
    "Goa",
    "Drum & Bass",
    "Club House",
    "Hardcore",
    "Terror",
    "Indie",
    "BritPop",
    "NegerPunk",
    "Polsk Punk",
    "Beat",
    "Christian Gangsta",
    "Heavy Metal",
    "Black Metal",
    "Crossover",
    "Contemporary C",
    "Christian Rock",
    "Merengue",
    "Salsa",
    "Thrash Metal",
    "Anime",
    "JPop",
    "SynthPop"
};

typedef struct id3 {
    char SongTitle[256];    /* +  3 30 */
    char Artist[256];       /* + 33 30 */
    char Album[256];        /* + 63 30 */
    char Year[256];         /* + 93  4 */
    char Comment[256];      /* + 97 30 */
    int  Genre;             /* +127  1 */
} id3_t;


#define NUM_GENRE (sizeof(GenreTbl)/(sizeof(char *)))

void GetId3String(char *buf, id3_t *id3p)
{
    memset(id3p,   0, sizeof(id3_t));

    strncpy(id3p->SongTitle, &buf[3],  30);
    strncpy(id3p->Artist,    &buf[33], 30);
    strncpy(id3p->Album,     &buf[63], 30);
    strncpy(id3p->Year,      &buf[93],  4);
    strncpy(id3p->Comment,   &buf[97], 30);
    id3p->Genre = (unsigned char)buf[127];
}

int main(int a, char *b[])
{
    int  i;
    int  vf = 0;		/* verbose */
    int  size;
    char *filename = NULL;
    FILE *fp;
    char buf[2048];
    struct stat stbuf;
    id3_t id3buf;

    memset(&id3buf,   0, sizeof(id3_t));

    for (i = 1; i<a; i++) {
        if (!strncmp(b[i],"-v",2)) {
            vf = 1;
            continue;
        }
        filename = b[i];
    }

    if (filename) {
	if (!(fp = fopen(filename,"rb"))) {
		perror("fopen");
		exit(1);
	}
	stat(filename, &stbuf);
	size = stbuf.st_size;
	if (size < TAG_SIZE) {
            printf("No ID3 Tag in this file. (size=%d)\n", size);
            exit(1);
        }
    } else {
        fp = stdin;
    }
    if (fseek(fp, size-128, SEEK_SET) < 0) {
        perror("fseek");
        if (fp != stdin) fclose(fp);
        exit(1);
    }
    if (fread(buf, 1, TAG_SIZE, fp) != TAG_SIZE) {
        perror("fread");
        if (fp != stdin) fclose(fp);
        exit(1);
    }

    if (strncmp(buf, "TAG", 3)) {
        printf("No ID3 Tag in this file.\n");
        exit(1);
    }

    GetId3String(buf, &id3buf);

    printf("Title  : %s\n", id3buf.SongTitle);
    printf("Artist : %s\n", id3buf.Artist);
    printf("Album  : %s\n", id3buf.Album);
    printf("Year   : %s\n", id3buf.Year);
    printf("Comment: %s\n", id3buf.Comment);
    printf("Genre  : %s (%d)\n", (id3buf.Genre < NUM_GENRE) ? GenreTbl[id3buf.Genre] : "Unknown", id3buf.Genre);

    if (fp != stdin) fclose(fp);
    return 0;
}
