#ifndef INCLUDED_READ_WORKBOOK_H
#define INCLUDED_READ_WORKBOOK_H

#include <read_relationships.h>

struct Sheet {
  XML_Char *name;
  XML_Char *sheetId;
  XML_Char *path_name;
  char isHidden;
  char hasMergedCells;
  unsigned short num_of_chunks;
  struct ArrayRelationships array_worksheet_rels;
  struct ArrayRelationships array_drawing_rels;
};

struct ArraySheets {
  unsigned short int length;
  struct Sheet **sheets;
};

extern XML_Parser xmlparser;
extern struct ArraySheets array_sheets;

void workbook_start_element(void *userData, const XML_Char *name, const XML_Char **attrs);
void workbook_end_element(void *userData, const XML_Char *name);
void sheet_main_start_element(void *userData, const XML_Char *name, const XML_Char **attrs);
void sheet_main_end_element(void *userData, const XML_Char *name);

#endif
