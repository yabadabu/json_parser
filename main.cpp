#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "json_parser.h"

// No spaces, no new lines,
void dumpCompressed(const TJsonParser::TObj *obj) {
  if (obj->isLiteral()) {
    if (obj->literal.type == TJsonParser::LITERAL_STRING) {
      // Will need to escape certain characters
      printf("\"%s\"", obj->literal.text);
    } 
    else
      printf("%s", obj->literal.text);
  }
  else if (obj->isObject()) {
    printf("{");
    for (size_t i = 0; i < obj->object.len; ++i) {
      if ( i )
        printf(",");
      printf("\"%s\":", obj->object.keys[i]);
      dumpCompressed(obj->object.values[i]);
    }
    printf("}");
  }
  else if (obj->isArray()) {
    printf("[" );
    for (size_t i = 0; i < obj->array.len; ++i) {
      if (i)
        printf(",");
      dumpCompressed(obj->array.values[i]);
    }
    printf("]");
  }
}

// ---------------------------------------------------------------------
void dump(int level, const TJsonParser::TObj *obj) {
  static const int tabs = 4;
  if (obj->isLiteral()) {
    printf("%*s%s (%d)\n", level * tabs, " ", obj->literal.text, obj->literal.type);
  }
  else if (obj->isObject()) {
    printf("%*s{\n", level * tabs, "  ");
    for (size_t i = 0; i < obj->object.len; ++i) {
      printf("%*s:%s: \n", ( level + 1 ) * tabs, " ", obj->object.keys[ i ]);
      dump(level + 1, obj->object.values[i]);
    }
    printf("%*s}\n", level * tabs, "  ");
  }
  else if (obj->isArray() ) {
    printf("%*s[\n", level * tabs, "  ");
    for (size_t i = 0; i < obj->array.len; ++i) {
      dump(level + 1, obj->array.values[i]);
    }
    printf("%*s]\n", level * tabs, "  ");
  }
}

// ------------------------------------------------------------
void dumpApp(const TJsonParser::TObj *obj) {

  if (!obj->isObject())
    return;
  const TJsonParser::TObj *pages = obj->object.get("pages");
  if (!pages->isArray())
    return;
  printf("There are %ld pages\n", pages->array.len);
  for (size_t i = 0; i < pages->array.len; ++i) {
    const TJsonParser::TObj *p = pages->array.at(i);
    if (!p->isObject())
      continue;
    float xmin = p->getFloat("left", 0);
    float ymin = p->getFloat("top", 0);
    float w = p->getFloat("width", 100);
    float h = p->getFloat("height", 100);
    bool active = p->getBool("active", true);
    const char *id = p->getStr("id");
    printf("Page %s at (%f,%f)-(%f,%f) active:%s\n", id, xmin, ymin, w, h, active ? "yes" : "no");
  }
}

// ------------------------------------------------------------
struct TBuffer {
  char*  data;
  size_t nbytes;
  TBuffer() : data(NULL), nbytes(0) { }
  ~TBuffer() {
    if (data)
      delete[] data, data = NULL;
  }
  bool loadFromFile(const char *infilename) {
    assert(data == NULL);
    FILE *f = fopen(infilename, "rb");
    if (!f)
      return false;
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    nbytes = file_size;
    data = new char[file_size];
    size_t bytes_read = fread(data, 1, nbytes, f);
    fclose(f);
    assert(bytes_read == nbytes);
    return true;
  }
};

// ------------------------------------------------------------
int main(int argc, char **argv) {
  
  const char *infilename = "sample.json";

  printf("Sizeof( TObj ) = %ld\n", sizeof(TJsonParser::TObj));

  TBuffer buf;
  if (!buf.loadFromFile(infilename))
    return -1;

  TJsonParser json;
  const TJsonParser::TObj *obj = json.parse( buf.data, buf.nbytes );
  if (!obj) {
    printf("-- Parsed KO: %s\n", json.getErrorText());
    return -2;
  }

  printf("-- Parsed OK ------------------- \n");
  dump(0, obj);

  printf("-- Compressed JSON ------------------- \n");
  dumpCompressed(obj);
  printf("\n");

  printf("-- Parsed by the app ----------------- \n");
  dumpApp(obj);
  return 0;
}

