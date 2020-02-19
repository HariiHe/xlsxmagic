#include <expat.h>



struct Col {
  unsigned short int min;
  unsigned short int max;
  float width;
  char isHidden;
};

struct ArrayCols {
  unsigned short int length;
  struct Col **cols;
};

extern char start_col; // A
extern char *end_col;
extern char start_row; // 1
extern char *end_row;

extern XML_Parser xmlparser;
extern struct ArrayCols array_cols;


void worksheet_start_element(void *userData, const XML_Char *name, const XML_Char **attrs);
void worksheet_end_element(void *userData, const XML_Char *name);
void col_start_element(void *userData, const XML_Char *name, const XML_Char **attrs);
void col_end_element(void *userData, const XML_Char *name);
