#ifndef INCLUDED_PRIVATE_H
#define INCLUDED_PRIVATE_H


#include <zip.h>
#include <expat.h>

#ifdef XML_LARGE_SIZE
#  define XML_FMT_INT_MOD "ll"
#else
#  define XML_FMT_INT_MOD "l"
#endif

#ifdef XML_UNICODE_WCHAR_T
#  include <wchar.h>
#  define XML_FMT_STR "ls"
#else
#  define XML_FMT_STR "s"
#endif

struct Color {
  XML_Char *rgb;
};

struct Font {
  float sz;
  XML_Char *name;
  char isBold;
  char isItalic; 
  XML_Char *underline;
  struct Color color;
};

extern const char *OUTPUT_FILE_NAME;
extern const char *OUTPUT_DIR;
extern const char *ORIGIN_FILE_PATH;
extern const char *TEMP_DIR;

# define CHUNKS_DIR_NAME "chunks"
# define BASE_CSS_FILE_NAME "base.css"
# define BASE_JS_FILE_NAME "xlsxmagic.js"
# define MANIFEST_FILE_NAME "manifest"
# define TEMPLATES_DIR_NAME "templates"

#define TYPE_DRAWING "http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing"

#endif
