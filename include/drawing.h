#ifndef INCLUDED_DRAWING_H
#define INCLUDED_DRAWING_H

#include <expat.h>
#include <relationship.h>
#include <zip.h>

struct Offset {
  unsigned int col;
  size_t colOff;
  unsigned int row;
  size_t rowOff;
};

struct Pic {
  XML_Char *name;
  char *hlinkClick_id;
  char *blip_embed;
  size_t cx;
  size_t cy;
};

struct GraphicFrame {
  XML_Char *name;
  size_t cx;
  size_t cy;
  char *chart_id;
};

struct TwoCellAnchor {
  char *editAs;
  struct Offset from;
  struct Offset to;
  struct Pic pic;
  struct GraphicFrame graphic_frame;
};

struct ChartMetaData {
  char *file_name;
  char *file_path;
  char *id;
};

struct ArrayChartMetaData {
  int length;
  struct ChartMetaData **chart_metadata;
};

struct DrawingCallbackData {
  struct ArrayRelationships *array_drawing_rels;
  struct TwoCellAnchor twocellanchor;
  struct Offset _tmp_offset;
  zip_t *zip;
  FILE *findexhtml;
  XML_Char *text;
  size_t textlen;
  XML_Char* skiptag;                    //tag to skip
  size_t skiptagcount;                  //nesting level for current tag to skip
  XML_StartElementHandler skip_start;   //start handler to set after skipping
  XML_EndElementHandler skip_end;       //end handler to set after skipping
  XML_CharacterDataHandler skip_data;   //data handler to set after skipping
  int index_sheet;
  int index_image;
  int index_graphicframe;
  char is_pic;
  char is_graphicframe;
  char *img_url;
  char *chart_url;
  struct ArrayChartMetaData array_chart_metadata;
};

struct  ArrayDrawingCallbackData {
  int length;
  struct DrawingCallbackData **arr_drawing_callbackdata;
};

extern XML_Parser xmlparser;

void drawings_start_element(void *callbackdata, const XML_Char *name, const XML_Char **attrs);
void drawings_end_element(void *callbackdata, const XML_Char *name);
void drawings_lv1_start_element(void *callbackdata, const XML_Char *name, const XML_Char **attrs);
void drawings_lv1_end_element(void *callbackdata, const XML_Char *name);
void drawings_lv2_start_element(void *callbackdata, const XML_Char *name, const XML_Char **attrs);
void drawings_lv2_end_element(void *callbackdata, const XML_Char *name);
void drawings_lv3_start_element(void *callbackdata, const XML_Char *name, const XML_Char **attrs);
void drawings_lv3_end_element(void *callbackdata, const XML_Char *name);
void drawings_lv4_start_element(void *callbackdata, const XML_Char *name, const XML_Char **attrs);
void drawings_lv4_end_element(void *callbackdata, const XML_Char *name);
void drawings_content_handler(void *callbackdata, const XML_Char *buf, int len);

void drawings_callbackdata_initialize(struct DrawingCallbackData *, struct ArrayRelationships *, FILE *, zip_t *, int);
struct TwoCellAnchor new_twocellanchor();

void drawings_skip_tag_start_element(void *callbackdata, const XML_Char *name, const XML_Char **attrs);
void drawings_skip_tag_end_element(void *callbackdata, const XML_Char *name);

#endif
