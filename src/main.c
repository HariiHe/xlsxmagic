#include <main.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include <private.h>
#include <stdio.h>
#include <version.h>

XML_Parser xmlparser;
const char *ORIGIN_FILE_PATH;
const char *OUTPUT_DIR;
const char *OUTPUT_FILE_NAME;
const char *TEMP_DIR;
const char *RESOURCE_URL;
const char *WORKING_DIR;
const char *CHUNKS_DIR_PATH;
const char *SHAREDSTRINGS_HTML_FILE_PATH;

int err;

int mkdir_p(const char *path, mode_t mode) {
  /* Adapted from http://stackoverflow.com/a/2336245/119527 */
  const size_t len = XML_Char_len(path);
  char _path[PATH_MAX];
  char *p;

  errno = 0;

  /* Copy string so its mutable */
  if (len > sizeof(_path)-1) {
      errno = ENAMETOOLONG;
      return -1;
  }
  strcpy(_path, path);

  /* Iterate the string */
  for (p = _path + 1; *p; p++) {
    if (*p == '/') {
      /* Temporarily truncate */
      *p = '\0';

      if (mkdir(_path, mode) != 0) {
        if (errno != EEXIST)
          return -1;
        }

      *p = '/';
    }
  }

  if (mkdir(_path, mode) != 0) {
    if (errno != EEXIST)
      return -1;
  }

  return 0;
}

zip_t *open_zip(const char *file_name) {
  return zip_open(file_name, ZIP_RDONLY, &err);
}

zip_file_t *open_zip_file(zip_t *zip, const char *zip_file_name) {
  return zip_fopen(zip, zip_file_name, ZIP_FL_UNCHANGED);
}

int load_relationships(zip_t *zip, char *zip_file_name, void *callbackdata) {
  zip_file_t *archive = open_zip_file(zip, zip_file_name);
  zip_error_t *err_zip = zip_get_error(zip);
  if (archive == NULL) {
    debug_print("%s: %s\n", zip_error_strerror(err_zip), zip_file_name);
    return -1;
  }
  int status = process_zip_file(archive, callbackdata, NULL, rels_start_element, rels_end_element);
  return status;
}

int load_drawings(zip_t *zip, char *zip_file_name, void *callbackdata) {
  zip_file_t *archive = open_zip_file(zip, zip_file_name);
  zip_error_t *err_zip = zip_get_error(zip);
  if (archive == NULL) {
    debug_print("%s: %s\n", zip_error_strerror(err_zip), zip_file_name);
    return -1;
  }
  int status = process_zip_file(archive, callbackdata, NULL, drawings_start_element, drawings_end_element);
  return status;
}

int load_chart(zip_t *zip, char *zip_file_name, void *callbackdata) {
  zip_file_t *archive = open_zip_file(zip, zip_file_name);
  zip_error_t *err_zip = zip_get_error(zip);
  if (archive == NULL) {
    debug_print("%s: %s\n", zip_error_strerror(err_zip), zip_file_name);
    return -1;
  }
  int status = process_zip_file(archive, callbackdata, NULL, chart_start_element, chart_end_element);
  return status;
}

int load_workbook(zip_t *zip) {
  const char *zip_file_name = "xl/workbook.xml";
  zip_file_t *archive = open_zip_file(zip, zip_file_name);
  int status = process_zip_file(archive, NULL, NULL, workbook_start_element, workbook_end_element);
  for(int i = 0; i < array_sheets.length; i++) {
    //35: xl/worksheets/_rels/sheet%d.xml.rels
    int len_zip_sheet_rels_file_name = XML_Char_len(array_sheets.sheets[i]->sheetId) + 35;
    char *zip_sheet_rels_file_name = XML_Char_malloc(len_zip_sheet_rels_file_name + 1);
    snprintf(zip_sheet_rels_file_name, len_zip_sheet_rels_file_name + 1, "xl/worksheets/_rels/sheet%s.xml.rels", array_sheets.sheets[i]->sheetId);
    array_sheets.sheets[i]->array_worksheet_rels.length = 0;
    array_sheets.sheets[i]->array_worksheet_rels.relationships = NULL;
    int status_sheet_rels = load_relationships(zip, zip_sheet_rels_file_name, &array_sheets.sheets[i]->array_worksheet_rels);
    free(zip_sheet_rels_file_name);
    if (status_sheet_rels != 1) {
      continue;
    }
  }
  return status;
}

void destroy_styles() {
  for (int i = 0; i < array_numfmts.length; i++) {
    free(array_numfmts.numfmts[i].formatCode);
    free(array_numfmts.numfmts[i].numFmtId);
  }
  free(array_numfmts.numfmts);
  for (int i = 0; i < array_fonts.length; i++) {
    free(array_fonts.fonts[i].name);
    free(array_fonts.fonts[i].underline);
    free(array_fonts.fonts[i].color.rgb);
  }
  free(array_fonts.fonts);
  for (int i = 0; i < array_fills.length; i++) {
    free(array_fills.fills[i].patternFill.patternType);
    free(array_fills.fills[i].patternFill.bgColor.rgb);
    free(array_fills.fills[i].patternFill.fgColor.rgb);
  }
  free(array_fills.fills);
  for (int i = 0; i < array_borders.length; i++) {
    free(array_borders.borders[i].left.style);
    free(array_borders.borders[i].left.color.rgb);
    free(array_borders.borders[i].right.style);
    free(array_borders.borders[i].right.color.rgb);
    free(array_borders.borders[i].top.style);
    free(array_borders.borders[i].top.color.rgb);
    free(array_borders.borders[i].bottom.style);
    free(array_borders.borders[i].bottom.color.rgb);
  }
  free(array_borders.borders);
  for (int i = 0; i < array_cellStyleXfs.length; i++) {
    free(array_cellStyleXfs.Xfs[i].alignment.horizontal);
    free(array_cellStyleXfs.Xfs[i].alignment.vertical);
    free(array_cellStyleXfs.Xfs[i].alignment.textRotation);
  }
  free(array_cellStyleXfs.Xfs);
  for (int i = 0; i < array_cellXfs.length; i++) {
    free(array_cellXfs.Xfs[i].alignment.horizontal);
    free(array_cellXfs.Xfs[i].alignment.vertical);
    free(array_cellXfs.Xfs[i].alignment.textRotation);
  }
  free(array_cellXfs.Xfs);
}

int load_styles(zip_t *zip) {
  const char *zip_file_name = "xl/styles.xml";
  zip_file_t *archive = open_zip_file(zip, zip_file_name);
  zip_error_t *err_zip = zip_get_error(zip);
  if (archive == NULL) {
    debug_print("%s: %s\n", zip_error_strerror(err_zip), zip_file_name);
    return -1;
  }
  // Load NumFMT first
  int status = process_zip_file(archive, NULL, NULL, styles_start_element, styles_end_element);
  return status;
}

int load_worksheets(zip_t *zip) {
  for(int i = 0; i < array_sheets.length; i++) {
    zip_file_t *archive = open_zip_file(zip, array_sheets.sheets[i]->path_name);
    zip_error_t *err_zip = zip_get_error(zip);
    if (archive == NULL) {
      debug_print("%s: %s\n", zip_error_strerror(err_zip), array_sheets.sheets[i]->path_name);
      return -1;
    }
    struct WorkSheet worksheet;
    worksheet.start_col = 'A';
    worksheet.start_row = '1';
    worksheet.end_col = '\0';
    worksheet.end_row = '\0';
    worksheet.index_sheet = i;
    worksheet.hasMergedCells = '0';
    worksheet.array_drawingids.length = 0;
    worksheet.array_drawingids.drawing_ids = NULL;
    worksheet.num_of_chunks = 1;

    int status_worksheet = process_zip_file(archive, &worksheet, NULL, worksheet_start_element, worksheet_end_element);
    if (status_worksheet != 1){
      return status_worksheet;
    }
    array_sheets.sheets[i]->hasMergedCells = worksheet.hasMergedCells;
    array_sheets.sheets[i]->array_drawing_rels.length = 0;
    array_sheets.sheets[i]->array_drawing_rels.relationships = NULL;
    array_sheets.sheets[i]->num_of_chunks = worksheet.num_of_chunks;

    for (int index_rels = 0; index_rels < array_sheets.sheets[i]->array_worksheet_rels.length; index_rels++) {
      for (int index_drawingid = 0; index_drawingid < worksheet.array_drawingids.length; index_drawingid++) {
        if (XML_Char_icmp(array_sheets.sheets[i]->array_worksheet_rels.relationships[index_rels]->id, worksheet.array_drawingids.drawing_ids[index_drawingid]) == 0) {
	  if (XML_Char_icmp(array_sheets.sheets[i]->array_worksheet_rels.relationships[index_rels]->type, TYPE_DRAWING) == 0) {
	    //23: xl/drawings/_rels/<token>.rels
	    int count = 0;
	    char *_tmp_target = strdup(array_sheets.sheets[i]->array_worksheet_rels.relationships[index_rels]->target);
	    char *token = strtok(_tmp_target, "/");
	    count++;
	    while (count <= 2) {
	      token = strtok(NULL, "/");
	      count++;
	    }
            int len_zip_drawing_rels = XML_Char_len(token) + 23;
	    char *zip_drawing_rels_file_name = XML_Char_malloc(len_zip_drawing_rels + 23 + 1);
	    snprintf(zip_drawing_rels_file_name, len_zip_drawing_rels + 1, "xl/drawings/_rels/%s.rels", token);
            int status_drawing_rels = load_relationships(zip, zip_drawing_rels_file_name, &array_sheets.sheets[i]->array_drawing_rels);
	    if (status_drawing_rels == -1) {
	     //TODO: Handle error
	    }
	    free(zip_drawing_rels_file_name);
	    free(_tmp_target);
	    break;
	  } else {
            int _tmp_length = array_sheets.sheets[i]->array_worksheet_rels.length;
            if (index_rels < _tmp_length - 1) {
              memmove(
                array_sheets.sheets[i]->array_worksheet_rels.relationships + index_rels,
                array_sheets.sheets[i]->array_worksheet_rels.relationships + index_rels + 1,
                (_tmp_length - index_rels - 1) * sizeof(struct Relationship *)
              );
              index_rels--;
	    } else {
              free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]->id);
              free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]->target);
              free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]->type);
              free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]);
              array_sheets.sheets[i]->array_worksheet_rels.relationships = XML_Char_realloc(
                array_sheets.sheets[i]->array_worksheet_rels.relationships,
                (_tmp_length - 1) * sizeof(struct Relationship *)
	      );
	    }
            array_sheets.sheets[i]->array_worksheet_rels.length--;
	    break;
	  }
        }
        if (index_drawingid == worksheet.array_drawingids.length - 1) {
          int _tmp_length = array_sheets.sheets[i]->array_worksheet_rels.length;
          if (index_rels < _tmp_length - 1) {
	    free(array_sheets.sheets[i]->array_worksheet_rels.relationships[index_rels]->id);
	    free(array_sheets.sheets[i]->array_worksheet_rels.relationships[index_rels]->target);
	    free(array_sheets.sheets[i]->array_worksheet_rels.relationships[index_rels]->type);
	    free(array_sheets.sheets[i]->array_worksheet_rels.relationships[index_rels]);
            memmove(
              array_sheets.sheets[i]->array_worksheet_rels.relationships + index_rels,
              array_sheets.sheets[i]->array_worksheet_rels.relationships + index_rels + 1,
              (_tmp_length - index_rels - 1) * sizeof(struct Relationship*)
            );
            index_rels--;
	  } else {
            free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]->id);
            free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]->target);
            free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]->type);
            free(array_sheets.sheets[i]->array_worksheet_rels.relationships[_tmp_length - 1]);
	    array_sheets.sheets[i]->array_worksheet_rels.relationships = XML_Char_realloc(
              array_sheets.sheets[i]->array_worksheet_rels.relationships,
	      (_tmp_length - 1) * sizeof(struct Relationship *)
	    );
	  }
          array_sheets.sheets[i]->array_worksheet_rels.length--;
        }
      }
    }

    for (int index_drawingid = 0; index_drawingid < worksheet.array_drawingids.length; index_drawingid++) {
      free(worksheet.array_drawingids.drawing_ids[index_drawingid]);
    }
    free(worksheet.array_drawingids.drawing_ids);
    free(worksheet.end_row);
    free(worksheet.end_col);
  }
  free(sharedStrings_position.positions);
  return 1;
}

int load_sharedStrings(zip_t *zip) {
  const char *zip_file_name = "xl/sharedStrings.xml";
  int len_sharedStrings_html_file_name = XML_Char_len(OUTPUT_FILE_NAME) + XML_Char_len(SHAREDSTRINGS_HTML_FILE_SUFFIX);
  char *SHAREDSTRINGS_HTML_FILE_NAME = XML_Char_malloc(len_sharedStrings_html_file_name + 1);
  snprintf(SHAREDSTRINGS_HTML_FILE_NAME, len_sharedStrings_html_file_name + 1, "%s%s", OUTPUT_FILE_NAME, SHAREDSTRINGS_HTML_FILE_SUFFIX);
  int len_sharedStrings_file_path = XML_Char_len(TEMP_DIR) + 1 + len_sharedStrings_html_file_name;
  char *_SHAREDSTRINGS_HTML_FILE_PATH = XML_Char_malloc(len_sharedStrings_file_path + 1);
  snprintf(_SHAREDSTRINGS_HTML_FILE_PATH, len_sharedStrings_file_path + 1, "%s/%s", TEMP_DIR, SHAREDSTRINGS_HTML_FILE_NAME);
  free(SHAREDSTRINGS_HTML_FILE_NAME);
  SHAREDSTRINGS_HTML_FILE_PATH = strdup(_SHAREDSTRINGS_HTML_FILE_PATH);
  free(_SHAREDSTRINGS_HTML_FILE_PATH);
  FILE *sharedStrings_file;
  sharedStrings_file = fopen(SHAREDSTRINGS_HTML_FILE_PATH, "wb+");
  if (sharedStrings_file == NULL) {
    debug_print("%s: %s\n", strerror(errno), SHAREDSTRINGS_HTML_FILE_PATH);
    return -1;
  }
  zip_file_t *archive = open_zip_file(zip, zip_file_name);
  zip_error_t *err_zip = zip_get_error(zip);
  if (archive == NULL) {
    debug_print("%s: %s\n", zip_error_strerror(err_zip), zip_file_name);
    return -1;
  }
  int status_sharedStrings = process_zip_file(archive, sharedStrings_file, NULL, sharedStrings_main_start_element, sharedStrings_main_end_element);
  if (status_sharedStrings == -1) {
    debug_print("%s\n", strerror(errno));
    fclose(sharedStrings_file);
    return -1;
  }
  fclose(sharedStrings_file);
  return 1;
}

int process_zip_file(zip_file_t *archive, void *callbackdata, XML_CharacterDataHandler content_handler, XML_StartElementHandler start_element, XML_EndElementHandler end_element) {
  void *buf;
  zip_int64_t buflen;
  xmlparser = XML_ParserCreate(NULL);
  int done;
  enum XML_Status status = XML_STATUS_ERROR;
  XML_SetUserData(xmlparser, callbackdata);
  XML_SetElementHandler(xmlparser, start_element, end_element);
  XML_SetCharacterDataHandler(xmlparser, content_handler);
  buf = XML_GetBuffer(xmlparser, PARSE_BUFFER_SIZE);
  while(buf && (buflen = zip_fread(archive, buf, PARSE_BUFFER_SIZE)) >= 0) {
    done = buflen < PARSE_BUFFER_SIZE;
    if((status = XML_ParseBuffer(xmlparser, (int)buflen, (done ? 1 : 0))) == XML_STATUS_ERROR) {
      debug_print("%" XML_FMT_STR " at line %" XML_FMT_INT_MOD "u\n",
              XML_ErrorString(XML_GetErrorCode(xmlparser)),
              XML_GetCurrentLineNumber(xmlparser));
       XML_ParserFree(xmlparser); 
       zip_fclose(archive);
       return 0;
    }
    if(done) {
      break;
    }
    buf = XML_GetBuffer(xmlparser, PARSE_BUFFER_SIZE);
  }
  XML_ParserFree(xmlparser); 
  zip_fclose(archive);
  return 1;
}

void embed_css(FILE *f, const char *css_path) {
  const char *_css_path = css_path;
  FILE *fcss;
  fcss = fopen(_css_path, "rb");
  if (fcss == NULL) {
    debug_print("%s: %s\n", strerror(errno), css_path);
  }
  char c;
  while ((c = fgetc(fcss)) != EOF) {
    fputc(c, f);
  }
  fclose(fcss);
}

void embed_js(FILE *f, const char *js_path) {
  const char *_js_path = js_path;
  FILE *fjs;
  fjs = fopen(_js_path, "rb");
  if (fjs == NULL) {
    debug_print("%s: %s\n", strerror(errno), js_path);
  }
  char c;
  while ((c = fgetc(fjs)) != EOF) {
    fputc(c, f);
  }
  fclose(fjs);
}

void destroy_workbook() {
  for (int index_sheet = 0; index_sheet < array_sheets.length; index_sheet++) {
    free(array_sheets.sheets[index_sheet]->name);
    free(array_sheets.sheets[index_sheet]->sheetId);
    free(array_sheets.sheets[index_sheet]->path_name);
    free(array_sheets.sheets[index_sheet]);
  }
  free(array_sheets.sheets);
}

// Generate index html file
void pre_process(zip_t *zip) {
  int len_templates_dir_path = XML_Char_len(WORKING_DIR) + XML_Char_len(TEMPLATES_DIR_NAME) + 1;
  char *TEMPLATES_DIR_PATH = XML_Char_malloc(len_templates_dir_path + 1);
  snprintf(TEMPLATES_DIR_PATH, len_templates_dir_path + 1, "%s/%s", WORKING_DIR, TEMPLATES_DIR_NAME);
  int len_base_css_path = len_templates_dir_path + XML_Char_len(BASE_CSS_FILE_NAME) + 1;
  char *BASE_CSS_PATH = XML_Char_malloc(len_base_css_path + 1);
  snprintf(BASE_CSS_PATH, len_base_css_path + 1, "%s/%s", TEMPLATES_DIR_PATH, BASE_CSS_FILE_NAME);
  int len_base_js_path = len_templates_dir_path + XML_Char_len(BASE_JS_FILE_NAME) + 1; 
  char *BASE_JS_PATH = XML_Char_malloc(len_base_js_path + 1);
  snprintf(BASE_JS_PATH, len_base_js_path + 1, "%s/%s", TEMPLATES_DIR_PATH, BASE_JS_FILE_NAME);
  int len_manifest_path = len_templates_dir_path + XML_Char_len(MANIFEST_FILE_NAME) + 1;
  char *MANIFEST_PATH = XML_Char_malloc(len_manifest_path + 1);
  snprintf(MANIFEST_PATH, len_manifest_path + 1, "%s/%s", TEMPLATES_DIR_PATH, MANIFEST_FILE_NAME);
  //5: .html
  int len_index_html_path = XML_Char_len(OUTPUT_DIR) + XML_Char_len(OUTPUT_FILE_NAME) + 5 + 1;
  char *INDEX_HTML_PATH = XML_Char_malloc(len_index_html_path + 1);
  snprintf(INDEX_HTML_PATH, len_index_html_path + 1, "%s/%s.html", OUTPUT_DIR, OUTPUT_FILE_NAME);
  FILE *fmanifest;
  fmanifest = fopen(MANIFEST_PATH, "rb");
  if (fmanifest == NULL) {
    debug_print("%s: %s\n", strerror(errno), MANIFEST_PATH);
    return;
  }
  FILE *findexhtml;
  findexhtml = fopen(INDEX_HTML_PATH, "ab+");
  if (findexhtml == NULL) {
    debug_print("%s: %s\n", strerror(errno), INDEX_HTML_PATH);
    return;
  }
  int len_chunks_dir_path = XML_Char_len(CHUNKS_DIR_PATH);

  char line[256];
  while(fgets(line, sizeof(line), fmanifest)) {
    if (line[0] == '\n') {
      continue;
    } else if (line[0] == '#') {
      continue;
    } else if (line[0] == '@') {
      if (XML_Char_icmp(line, "@base.min.css\n") == 0) {
	fputs("<style>", findexhtml);
        embed_css(findexhtml, BASE_CSS_PATH);
	fputs("</style>", findexhtml);
      } else if (XML_Char_icmp(line, "@xlsxmagic.min.js\n") == 0) {
	fputs("<script>", findexhtml);
        embed_js(findexhtml, BASE_JS_PATH);
	fputs("</script>", findexhtml);
      }
    } else if (line[0] == '$') {
      if (XML_Char_icmp(line, "$tables\n") == 0) {
	for (int index_sheet = 0; index_sheet < array_sheets.length; index_sheet++) {
	  int len_index_sheet = snprintf(NULL, 0, "%d", index_sheet);
	  int len_num_of_chunks = snprintf(NULL, 0, "%d", array_sheets.sheets[index_sheet]->num_of_chunks + 1);
	  int len_div_table = 47 + len_index_sheet + XML_Char_len(array_sheets.sheets[index_sheet]->name) + len_num_of_chunks;
	  char *DIV_TABLE = XML_Char_malloc(len_div_table + 1);
	  snprintf(
            DIV_TABLE, len_div_table + 1,
            "<div id=\"sheet_%d\" name=\"%s\" data-num-of-chunks=\"%d\">",
	    index_sheet, array_sheets.sheets[index_sheet]->name,
	    array_sheets.sheets[index_sheet]->num_of_chunks + 1
	  );
          fputs(DIV_TABLE, findexhtml);
	  free(DIV_TABLE);
	  fputs("\n", findexhtml);
	  for (int index_chunk = 0; index_chunk <= array_sheets.sheets[index_sheet]->num_of_chunks; index_chunk++) {
	    //7: chunk_%d_%d
	    int len_chunk_html_file_name = snprintf(NULL, 0, "%d", index_chunk) + len_index_sheet + 7;
	    char *CHUNK_HTML_FILE_NAME = XML_Char_malloc(len_chunk_html_file_name + 1);
	    snprintf(CHUNK_HTML_FILE_NAME, len_chunk_html_file_name + 1, "chunk_%d_%d", index_sheet, index_chunk);
	    int len_resource_url, len_chunk_html_url;
	    char *CHUNK_HTML_URL;
	    if (strstr(RESOURCE_URL, "https") != NULL) {
	      len_resource_url = XML_Char_len(RESOURCE_URL);
	      int len_output_file_name = XML_Char_len(OUTPUT_FILE_NAME);
	      len_chunk_html_url = len_chunk_html_file_name + len_resource_url + len_output_file_name + 7;
	      CHUNK_HTML_URL = XML_Char_malloc(len_chunk_html_url + 1);
	      snprintf(CHUNK_HTML_URL , len_chunk_html_url + 1, "%s%s/chunk/%s", RESOURCE_URL, OUTPUT_FILE_NAME, CHUNK_HTML_FILE_NAME);
	    } else {
	      len_chunk_html_url = len_chunk_html_file_name + len_chunks_dir_path + 7;
	      CHUNK_HTML_URL = XML_Char_malloc(len_chunk_html_url + 1);
	      snprintf(CHUNK_HTML_URL, len_chunk_html_url + 1, "%s/%s.chunk", CHUNKS_DIR_PATH, CHUNK_HTML_FILE_NAME);
	    }

	    int len_div_chunk = len_chunk_html_file_name + len_chunk_html_url + 35;
            char *DIV_CHUNK = XML_Char_malloc(len_div_chunk + 1);
	    snprintf(
              DIV_CHUNK, len_div_chunk + 1,
	      "<div id=\"%s\" data-chunk-url=\"%s\"></div>",
	      CHUNK_HTML_FILE_NAME, CHUNK_HTML_URL
	    );
	    free(CHUNK_HTML_URL);
            fputs(DIV_CHUNK, findexhtml);
	    free(DIV_CHUNK);
	    fputs("\n", findexhtml);
	    free(CHUNK_HTML_FILE_NAME);
	  }
          if (array_sheets.sheets[index_sheet]->hasMergedCells == '1') {
	    int len_chunk_mc_file_name = len_index_sheet + 9;
	    char *CHUNK_MC_FILE_NAME = XML_Char_malloc(len_chunk_mc_file_name + 1);
	    snprintf(CHUNK_MC_FILE_NAME, len_chunk_mc_file_name + 1, "chunk_%d_mc", index_sheet);
	    int len_resource_url, len_chunk_mc_url;
	    char *CHUNK_MC_URL;
	    if (strstr(RESOURCE_URL, "https") != NULL) {
	      len_resource_url = XML_Char_len(RESOURCE_URL);
	      int len_output_file_name = XML_Char_len(OUTPUT_FILE_NAME);
	      len_chunk_mc_url = len_chunk_mc_file_name + len_resource_url + len_output_file_name + 6;
	      CHUNK_MC_URL = XML_Char_malloc(len_chunk_mc_url + 1);
	      snprintf(CHUNK_MC_URL , len_chunk_mc_url + 1, "%s%s/json/%s", RESOURCE_URL, OUTPUT_FILE_NAME, CHUNK_MC_FILE_NAME);
	    } else {
	      len_chunk_mc_url = len_chunk_mc_file_name + len_chunks_dir_path + 6;
	      CHUNK_MC_URL = XML_Char_malloc(len_chunk_mc_url + 1);
	      snprintf(CHUNK_MC_URL, len_chunk_mc_url + 1, "%s/%s.json", CHUNKS_DIR_PATH, CHUNK_MC_FILE_NAME);
	    }
	    int len_div_chunk = len_chunk_mc_file_name + len_chunk_mc_url + 35;
            char *DIV_CHUNK = XML_Char_malloc(len_div_chunk + 1);
	    snprintf(
              DIV_CHUNK, len_div_chunk + 1,
	      "<div id=\"%s\" data-chunk-url=\"%s\"></div>",
	      CHUNK_MC_FILE_NAME, CHUNK_MC_URL
	    );
	    free(CHUNK_MC_URL);
            fputs(DIV_CHUNK, findexhtml);
	    free(DIV_CHUNK);
	    fputs("\n", findexhtml);
	    free(CHUNK_MC_FILE_NAME);
          }

	  for (int index_rels = 0; index_rels < array_sheets.sheets[index_sheet]->array_worksheet_rels.length; index_rels++) { 
            int len_zip_drawing_file_name = XML_Char_len(array_sheets.sheets[index_sheet]->array_worksheet_rels.relationships[index_rels]->target);
	    char *zip_drawing_file_name = XML_Char_malloc(len_zip_drawing_file_name + 1);
	    snprintf(
	      zip_drawing_file_name, len_zip_drawing_file_name + 1,
	      "xl%s", 
	      array_sheets.sheets[index_sheet]->array_worksheet_rels.relationships[index_rels]->target + 2
	    );
	    free(array_sheets.sheets[index_sheet]->array_worksheet_rels.relationships[index_rels]->id);
	    free(array_sheets.sheets[index_sheet]->array_worksheet_rels.relationships[index_rels]->target);
	    free(array_sheets.sheets[index_sheet]->array_worksheet_rels.relationships[index_rels]->type);
	    free(array_sheets.sheets[index_sheet]->array_worksheet_rels.relationships[index_rels]);
	    struct DrawingCallbackData drawing_callbackdata;
            drawings_callbackdata_initialize(&drawing_callbackdata, &array_sheets.sheets[index_sheet]->array_drawing_rels, findexhtml, zip, index_sheet);
	    int status_drawings = load_drawings(zip, zip_drawing_file_name, &drawing_callbackdata);
	    if (status_drawings != -1) {
	      //TODO: Handle error
	    }

	    for (int i_drawing_callback = 0; i_drawing_callback < drawing_callbackdata.array_chart_metadata.length; i_drawing_callback++) {
	      for (int index_drawing_rel = 0; index_drawing_rel < array_sheets.sheets[index_sheet]->array_drawing_rels.length; index_drawing_rel++) {
		if (XML_Char_icmp(
                  drawing_callbackdata.array_chart_metadata.chart_metadata[i_drawing_callback]->id,
		  array_sheets.sheets[index_sheet]->array_drawing_rels.relationships[index_drawing_rel]->id
		  ) == 0) {
                  int len_zip_chart_file_name = XML_Char_len(array_sheets.sheets[index_sheet]->array_drawing_rels.relationships[index_drawing_rel]->target);
		  char *zip_chart_file_name = XML_Char_malloc(len_zip_chart_file_name + 1);
		  snprintf(
                    zip_chart_file_name, len_zip_chart_file_name + 1, "xl%s",
		    array_sheets.sheets[index_sheet]->array_drawing_rels.relationships[index_drawing_rel]->target + 2
		  );
		  struct ChartCallBackData chart_callbackdata;
		  int status_init = chart_callbackdata_initialize(
                    &chart_callbackdata,
		    drawing_callbackdata.array_chart_metadata.chart_metadata[i_drawing_callback]->file_path,
		    index_sheet
		  );
		  if (status_init == -1) {
		    //TODO: Handle error
		    continue;
		  }
                  int status_chart = load_chart(zip, zip_chart_file_name, &chart_callbackdata);
		  free(zip_chart_file_name);
		  fclose(chart_callbackdata.fchart);
		}
	      }

	      free(drawing_callbackdata.array_chart_metadata.chart_metadata[i_drawing_callback]->id);
	      free(drawing_callbackdata.array_chart_metadata.chart_metadata[i_drawing_callback]->file_name);
	      free(drawing_callbackdata.array_chart_metadata.chart_metadata[i_drawing_callback]->file_path);
	      free(drawing_callbackdata.array_chart_metadata.chart_metadata[i_drawing_callback]);
	    }
	    free(drawing_callbackdata.array_chart_metadata.chart_metadata);
	    free(zip_drawing_file_name);
	  }
	  for (int index_drawing_rel = 0; index_drawing_rel < array_sheets.sheets[index_sheet]->array_drawing_rels.length; index_drawing_rel++) {
            free(array_sheets.sheets[index_sheet]->array_drawing_rels.relationships[index_drawing_rel]->id);
            free(array_sheets.sheets[index_sheet]->array_drawing_rels.relationships[index_drawing_rel]->target);
            free(array_sheets.sheets[index_sheet]->array_drawing_rels.relationships[index_drawing_rel]->type);
            free(array_sheets.sheets[index_sheet]->array_drawing_rels.relationships[index_drawing_rel]);
	  }
	  if (array_sheets.sheets[index_sheet]->array_drawing_rels.length !=0)
	    free(array_sheets.sheets[index_sheet]->array_drawing_rels.relationships);
	  if (array_sheets.sheets[index_sheet]->array_worksheet_rels.length !=0)
	    free(array_sheets.sheets[index_sheet]->array_worksheet_rels.relationships);
	  fputs("</div>", findexhtml);
	  fputs("\n", findexhtml);
        }
      } else if (XML_Char_icmp(line, "$buttons\n") == 0) {
        //<button id="btn-Form Responses 1">Form Responses 1</button>
	for (int index_sheet = 0; index_sheet < array_sheets.length; index_sheet++) {
	  char *BUTTON_HTML = NULL;
	  if (index_sheet == 0) {
	    int len_button_html = 1 + XML_Char_len(array_sheets.sheets[index_sheet]->name) + 87;
	    BUTTON_HTML = XML_Char_realloc(BUTTON_HTML, len_button_html + 1);
	    snprintf(
              BUTTON_HTML, len_button_html + 1,
	      "<button id=\"btn_%d\" style=\"font-weight:bold;\"onclick=\"handleButtonClick(event)\">%s</button>",
	      index_sheet, array_sheets.sheets[index_sheet]->name
	    );
	  } else {
	    int len_index_sheet = snprintf(NULL, 0, "%d", index_sheet);
	    int len_button_html = len_index_sheet + XML_Char_len(array_sheets.sheets[index_sheet]->name) + 62;
	    BUTTON_HTML = XML_Char_realloc(BUTTON_HTML, len_button_html + 1);
	    snprintf(
              BUTTON_HTML , len_button_html + 1,
	      "<button id=\"btn_%d\" onclick=\"handleButtonClick(event)\">%s</button>",
	      index_sheet, array_sheets.sheets[index_sheet]->name
	    );
	  }
	  fputs(BUTTON_HTML, findexhtml);
	  free(BUTTON_HTML);
	  fputs("\n", findexhtml);
          free(array_sheets.sheets[index_sheet]->name);
          free(array_sheets.sheets[index_sheet]->sheetId);
          free(array_sheets.sheets[index_sheet]->path_name);
          free(array_sheets.sheets[index_sheet]);
        }
        free(array_sheets.sheets);
      }
     } else {
      //Insert html statement
      fputs(line, findexhtml);
    }
  }
  fclose(fmanifest);
  fclose(findexhtml);
  free(MANIFEST_PATH);
  free(TEMPLATES_DIR_PATH);
  free(BASE_CSS_PATH);
  free(BASE_JS_PATH);
  free(INDEX_HTML_PATH);
}

void post_process() {
  //TODO: To handle remove redunant rows, columns, condition formating, create a redundant chunk to fill image in table
}

int main(int argc, char **argv) {
  debug_print("%s\n", XLSXMAGIC_FULLNAME);
  int c;
  int digit_optind = 0;
  char has_origin_file_path = '0';
  char has_output_dir = '0';
  char has_output_file_name = '0';
  char has_tmp_dir = '0';
  char has_url_resource = '0';

  while (1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
         {"origin-file-path", required_argument, 0, 0},
         {"output-dir",  required_argument, 0, 0},
         {"output-file-name", required_argument, 0, 0},
         {"tmp-dir", required_argument, 0, 0},
         {"url-resource", required_argument, 0, 0},
         {"help", no_argument, 0, 'h'},
         {"version", no_argument, 0, 'v'},
         {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "hv",
             long_options, &option_index);

    if (c == -1)
       break;

    switch (c) {
      case 0:
        printf("option %s", long_options[option_index].name);
	if (strcmp(long_options[option_index].name, "origin-file-path") == 0) {
	  if (optarg) {
	    ORIGIN_FILE_PATH = strdup(optarg);
	    has_origin_file_path = '1';
            printf(" with arg %s", optarg);
	  }
	} else if (strcmp(long_options[option_index].name, "output-dir") == 0) {
	  if (optarg) {
	    OUTPUT_DIR = strdup(optarg);
	    has_output_dir = '1';
            printf(" with arg %s", optarg);
	  }
	} else if (strcmp(long_options[option_index].name, "output-file-name") == 0) {
	  if (optarg) {
	    OUTPUT_FILE_NAME = strdup(optarg);
	    has_output_file_name = '1';
            printf(" with arg %s", optarg);
	  }
	} else if (strcmp(long_options[option_index].name, "tmp-dir") == 0) {
	  if (optarg) {
	    TEMP_DIR = strdup(optarg);
	    has_tmp_dir = '1';
            printf(" with arg %s", optarg);
	  }
	} else if (strcmp(long_options[option_index].name, "url-resource") == 0) {
	  if (optarg) {
	    RESOURCE_URL = strdup(optarg);
	    has_url_resource = '1';
            printf(" with arg %s", optarg);
	  }
	}
        printf("\n");
        break;

      case '0':
      case '1':
      case '2':
        if (digit_optind != 0 && digit_optind != this_option_optind)
          printf("digits occur in two different argv-elements.\n");
        digit_optind = this_option_optind;
        printf("option %c\n", c);
        exit(EXIT_SUCCESS);

      case 'h':
      case '?':
	printf("%s\n", XLSXMAGIC_FULLNAME);
	printf("%s%50s\n", "--origin-file-path", "Path to xlsx file to be converted");
	printf("%s%40s\n", "--output-dir", "Path to ouput dir");
	printf("%s%58s\n", "--output-file-name", "File index html (exclude .html extension)");
	printf("%s%42s\n", "--tmp-dir", "Path to temp dir");
	printf("%s%70s\n", "--url-resource", "Url to resource (image, etc) follow by enviroment");
	printf("%s%48s\n", "-h, --help", "Print usage information");
	printf("%s%42s\n", "-v, --version", "Show current version");
        exit(EXIT_SUCCESS);
      case 'v':
	printf("%s\n", XLSXMAGIC_FULLNAME);
        exit(EXIT_SUCCESS);

      default:
        printf("?? getopt returned character code 0%o ??\n", c);
        exit(EXIT_SUCCESS);
    }
  }

  if (optind < argc) {
    printf("non-option ARGV-elements: ");
    while (optind < argc)
      printf("%s ", argv[optind++]);
    printf("\n");
    goto LOAD_RESOURCES_FAILED;
  }
  //Set default to test on local
  if (has_origin_file_path == '0') {
    ORIGIN_FILE_PATH = "/home/huydang/Downloads/excelsample/Project_Management__codestringers.xlsx";
  }
  if (has_output_file_name == '0') {
    OUTPUT_FILE_NAME = "index";
  }
  char path[PATH_MAX] = {0};
  int dest_len = PATH_MAX;
  if (readlink("/proc/self/exe", path, dest_len) != -1) {
    dirname(path);
    //strcat(path, "/");
    debug_print("WORKING DIR: %s\n", path);
    WORKING_DIR = path;
  }
  if (has_output_dir == '0') {
    char *OUTPUT_DIR_NAME = "output";
    int len_tmp_output_dir = XML_Char_len(WORKING_DIR) + XML_Char_len(OUTPUT_DIR_NAME) + 1;
    char *_tmp_output_dir = XML_Char_malloc(len_tmp_output_dir + 1);
    snprintf(_tmp_output_dir, len_tmp_output_dir + 1, "%s/%s", WORKING_DIR, OUTPUT_DIR_NAME);
    OUTPUT_DIR = strdup(_tmp_output_dir);
    free(_tmp_output_dir);
  }
  if (has_tmp_dir == '0') {
    TEMP_DIR = "/tmp";
  }
  if (has_url_resource == '0') {
    RESOURCE_URL = strdup(OUTPUT_DIR);
  }

  zip_t *zip = open_zip(ORIGIN_FILE_PATH);
  if (zip == NULL){
    debug_print("%s: %s\n", strerror(errno), ORIGIN_FILE_PATH);
    goto OPEN_ZIP_FAILED;
  }

  // +1 for "/" +1 for '\0'
  struct stat st = {0};
  if (stat(OUTPUT_DIR, &st) == -1) {
    int status = mkdir_p(OUTPUT_DIR, 0755);
    if (status != 0) {
      debug_print("%s: %s\n", strerror(errno), OUTPUT_DIR);
      goto LOAD_RESOURCES_FAILED;
    }
  }
  int len_chunks_dir_path = XML_Char_len(OUTPUT_DIR) + XML_Char_len(CHUNKS_DIR_NAME) + 1;
  char *_tmp_chunks_dir_path = XML_Char_malloc(len_chunks_dir_path  + 1);
  if (_tmp_chunks_dir_path == NULL) {
    debug_print("%s\n", strerror(errno));
  }
  snprintf(_tmp_chunks_dir_path, len_chunks_dir_path + 1, "%s/%s", OUTPUT_DIR, CHUNKS_DIR_NAME);
  CHUNKS_DIR_PATH = strdup(_tmp_chunks_dir_path);
  free(_tmp_chunks_dir_path);
  if (stat(CHUNKS_DIR_PATH, &st) == -1) {
    int status = mkdir_p(CHUNKS_DIR_PATH, 0755);
    if (status != 0) {
      debug_print("%s: %s\n", strerror(errno), CHUNKS_DIR_PATH);
      goto LOAD_RESOURCES_FAILED;
    }
  }
  int status_workbook = load_workbook(zip);
  if (status_workbook != 1) {
    debug_print("%s\n", strerror(errno));
    goto LOAD_RESOURCES_FAILED;
  }
  int status_styles = load_styles(zip);
  if (status_styles != 1) {
    debug_print("%s\n", strerror(errno));
    destroy_workbook();
    goto LOAD_RESOURCES_FAILED;
  }
  int status_sharedStrings = load_sharedStrings(zip);
  if (status_sharedStrings != 1) {
    debug_print("%s\n", strerror(errno));
    destroy_styles();
    destroy_workbook();
    goto LOAD_RESOURCES_FAILED;
  }
  int status_worksheets = load_worksheets(zip);
  if (status_worksheets != 1) {
    debug_print("%s\n", strerror(errno));
    destroy_styles();
    destroy_workbook();
    goto LOAD_RESOURCES_FAILED;
  }
  destroy_styles();
  pre_process(zip);

LOAD_RESOURCES_FAILED:
  zip_close(zip);
OPEN_ZIP_FAILED:
  if (has_origin_file_path == '1')
    free((char *)ORIGIN_FILE_PATH);
  if (has_output_file_name == '1')
    free((char *)OUTPUT_FILE_NAME);
  if (has_tmp_dir == '1')
    free((char *)TEMP_DIR);
  free((char *)RESOURCE_URL);
  free((char *)CHUNKS_DIR_PATH);
  free((char *)OUTPUT_DIR);
  int status_clean_ss_data = clean_ss_data(SHAREDSTRINGS_HTML_FILE_PATH);
  if (status_clean_ss_data != 0)
    debug_print("%s\n", strerror(errno));
  free((char *)SHAREDSTRINGS_HTML_FILE_PATH);
  exit(EXIT_SUCCESS);
}
