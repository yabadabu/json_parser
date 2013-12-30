#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cctype>
#include "json_parser.h"

#define dbg_printf if( 0 ) printf

bool TJsonParser::checkKeyword(const char *word, size_t word_length) {

  if (top + word_length > end) {
    setErrorText("Unexpected end of stream.");
    return false;
  }

  // all those -1 are because top has already been advanced
  if (strncmp(top - 1, word, word_length) == 0) {
    last_str = allocStr(top - 1, word_length);
    top += word_length - 1;
    return true;
  }

  return false;
}

void TJsonParser::setErrorText(const char *new_error) {
  _snprintf(error_text, sizeof( error_text )-1, "At offset %d : %s. Near text %*s", top-1 - start, new_error, 10, top - 5);
}

TJsonParser::TOpCode TJsonParser::scanOpCode() {
  while (top != end) {
    char c = *top++;

    switch (c) {
    case '{': return MAP_OPEN;
    case ':': return MAP_KEY_SEPARATOR;
    case '}': return MAP_CLOSE;
    case '[': return ARRAY_OPEN;
    case ',': return COMMA_SEPARATOR;
    case ']': return ARRAY_CLOSE;
    case '"': {
      const char *text = top;
      while (top < end && *top != '"') {
        if (*top == '\\')
          ++top;
        ++top;
      }
      if (top >= end) {
        setErrorText("Unexpected end of stream parsing string.");
        return END_OF_STREAM;
      }
      last_str = allocStr(text, top - text);
      ++top;
      return STRING_VALUE;
    }

    case 'n':
      if (checkKeyword("null", 4))
        return NULL_VALUE;
      break;
    case 't':
      if (checkKeyword("true", 4))
        return BOOL_TRUE_VALUE;
      break;
    case 'f':
      if (checkKeyword("false", 5))
        return BOOL_FALSE_VALUE;
      break;
    }
    if (isalpha(c)) {
      last_str = top - 1;
      return INVALID_OPCODE;
    }
    if (isdigit(c) || c == '-' || c == '+' ) {

      // Need to take care of floats, e....
      const char *text = top - 1;
      while (top != end && (isdigit(*top) || *top == '.'))
        ++top;
      if (top == end) {
        setErrorText("Unexpected end of stream parsing number.");
        return END_OF_STREAM;
      }
      last_str = allocStr(text, top - text);
      return NUMERIC_VALUE;
    }
  }
  return END_OF_STREAM;
}

// ------------------------------------------------------------------
// { key1:obj, key2:obj, .. }
TJsonParser::TObjType TJsonParser::parseMap(TObj *obj) {

  obj->type = OBJ_MAP;
  obj->object.len = 0;
  obj->object.values = NULL;
  obj->object.keys = NULL;

  // Reserve some space
  size_t      nobjs = 0;
  const char *keys[max_children_per_node];
  TObj *      objs[max_children_per_node];

  dbg_printf("map {\n");

  TOpCode op = INVALID_OPCODE;
  while (top != end) {
    op = scanOpCode();

    // Just in case the map is empty
    if (op == MAP_CLOSE) {
      assert(nobjs == 0);
      dbg_printf("empty map }\n");
      return OBJ_MAP;
    }

    // The identifier must be a string literal
    if (op != STRING_VALUE) {
      setErrorText("Expecting identifier of key map.");
      return OBJ_INVALID;
    }

    keys[nobjs] = last_str;
    dbg_printf("Key   : (%d) %s\n", op, last_str);

    // Now the separator : between the key and the value
    op = scanOpCode();
    if (op != MAP_KEY_SEPARATOR) {
      setErrorText("Expecting a ':' while parsing map");
      return OBJ_INVALID;
    }

    // Then the object value
    dbg_printf("Value : ");
    TObj *child = allocObj();

    TObjType otype = parseObj(child);
    if (otype == OBJ_INVALID)
      return OBJ_INVALID;

    // If all is OK, save pointer and incr # objs
    assert(nobjs < max_children_per_node);
    objs[nobjs++] = child;

    // Check if another key will come or we have reach the end of the map
    op = scanOpCode();

    // Close the map
    if (op == MAP_CLOSE) {
      dbg_printf("map }\n");

      // Copy pointers 
      size_t bytes_for_pointers = nobjs * sizeof(TObj *);
      obj->object.len = nobjs;
      obj->object.values = (TObj **)allocData(bytes_for_pointers);
      obj->object.keys = (const char **)allocData(bytes_for_pointers);
      memcpy(obj->object.values, objs, bytes_for_pointers);
      memcpy(obj->object.keys, keys, bytes_for_pointers);
      return OBJ_MAP;
    }

    // Then we expect a ',', any other option is an error
    if (op != COMMA_SEPARATOR) {
      setErrorText("Expecting a ',' while parsing map");
      return OBJ_INVALID;
    }
  }

  return OBJ_INVALID;
}

// ------------------------------------------------------------------
// [ obj, obj, obj .. ]
TJsonParser::TObjType TJsonParser::parseArray(TObj *obj) {
  obj->type = OBJ_ARRAY;
  obj->array.len = 0;
  obj->array.values = NULL;

  size_t nobjs = 0;
  TObj *objs[max_children_per_node];

  // Check if the array is empty, else, restore the scan point
  const char *prev_top = top;
  TOpCode nop = scanOpCode();
  if (nop == ARRAY_CLOSE) {
    dbg_printf("empty array []\n");
    return OBJ_ARRAY;
  }
  top = prev_top;

  dbg_printf("array [\n");
  TOpCode op = INVALID_OPCODE;
  while (top != end) {

    // Allocate an object and parse it.
    TObj *child = allocObj();
    TObjType otype = parseObj(child);

    if (otype == OBJ_INVALID)
      return OBJ_INVALID;

    assert(nobjs < max_children_per_node);
    objs[nobjs++] = child;

    // Check if another array member will come or we have reach the end of the array
    op = scanOpCode();
    if (op == ARRAY_CLOSE) {
      dbg_printf("]\n");
      
      // Save pointers
      size_t bytes_for_pointers = nobjs * sizeof(TObj *);
      obj->array.len = nobjs;
      obj->array.values = (TObj **)allocData(bytes_for_pointers );
      memcpy(obj->array.values, objs, bytes_for_pointers);

      return OBJ_ARRAY;
    }

    // Then we expect a comma between each object in the array
    if (op != COMMA_SEPARATOR) {
      setErrorText("Expected ',' as array member separator while parsing array");
      return OBJ_INVALID;
    }

  }

  setErrorText("Unexpected end of stream parsing array.");
  return OBJ_INVALID;
}

// ------------------------------------------------------------------
// true|false|null|int|"string"
TJsonParser::TObjType TJsonParser::parseLiteral(TOpCode op, TObj *obj) {

  obj->type = OBJ_LITERAL;
  obj->literal.text = last_str;

  switch (op) {
  case BOOL_FALSE_VALUE: obj->literal.type = LITERAL_BOOL_FALSE; break;
  case BOOL_TRUE_VALUE:  obj->literal.type = LITERAL_BOOL_TRUE;  break;
  case NULL_VALUE:       obj->literal.type = LITERAL_NULL;       break;
  case STRING_VALUE:     obj->literal.type = LITERAL_STRING;     break;
  case NUMERIC_VALUE:    obj->literal.type = LITERAL_NUMERIC;    break;
  default:
    printf("Invalid literal type %d : %s", op, last_str);
    obj->literal.type = LITERAL_INVALID; break;
  }

  dbg_printf("(%d) %s\n", op, last_str);
  return OBJ_LITERAL;
}

// ---------------------------------------------------------
TJsonParser::TObjType TJsonParser::parseObj( TObj *obj ) {
  while (top != end) {
    TOpCode op = scanOpCode();
    if (op == END_OF_STREAM)
      break;
    else if (op == INVALID_OPCODE) {
      setErrorText("Invalid characters found");
      return OBJ_INVALID;
    }
    else if (op == MAP_OPEN) 
      return parseMap( obj );
    else if (op == ARRAY_OPEN) 
      return parseArray( obj );
    else
      return parseLiteral( op, obj );
  }
  setErrorText("Unexpected end of stream parsing object.");
  return OBJ_INVALID;
}

// ---------------------------------------------------------
const TJsonParser::TObj *TJsonParser::parse(const char *buf, size_t nbytes) {
  
  // Reset parsing pointer
  start = buf;
  end = buf + nbytes;
  top = buf;
  last_str = NULL;

  // Reset error msgs
  error_text[0] = 0x00;

  // Reset output buffer
  out_top = out_base;

  TObj *root = allocObj();
  TObjType otype = parseObj(root);
  printf("%ld bytes parsed. %ld/%ld bytes used\n", nbytes, getUsedBytes(), out_buffer_size);
  if (otype == OBJ_INVALID)
    return NULL;
  return root;
}

// ------------------------------------------------------------------------------
TJsonParser::TJsonParser( size_t buffer_size ) {
  out_buffer_size = buffer_size;
  out_base = new u8[ out_buffer_size ];
  out_top = out_base;
}

TJsonParser::~TJsonParser() {
  delete[] out_base;
}

// ------------------------------------------------------------------------------
const char *TJsonParser::allocStr(const char *buf, size_t nbytes) {
  char *dst = (char *)allocData(nbytes+1);    // +1 for the terminator
  // evaluate escaped sequences/unicodes/etc..
  memcpy(dst, buf, nbytes);
  dst[nbytes] = 0x0;
  return dst;
}

TJsonParser::TObj *TJsonParser::allocObj( ) {
  return (TObj *)allocData( sizeof( TObj ) );
}

void *TJsonParser::allocData(size_t nbytes) {
  nbytes = (nbytes + 3) & (~3);   // Keep data aligned to 4 bytes
  void *buf = out_top;
  out_top += nbytes;
  assert(getUsedBytes() < out_buffer_size);
  return buf;
}

// ------------------------------------------------------------------------------
const TJsonParser::TObj *TJsonParser::TObj::TAttributes::get(const char *akey) const {
  for (size_t i = 0; i < len; ++i) {
    if (strcmp(keys[i], akey) == 0)
      return values[i];
  }
  return NULL;
}

const TJsonParser::TObj *TJsonParser::TObj::TArray::at(size_t n) const {
  assert(n < len);
  return values[n];
}

// -------------------------------------------------------------------------------------
// helpers, assume the obj to be of type obj_map or will return the def_value
float       TJsonParser::TObj::getFloat(const char *key, float def_value) const {
  assert( this );
  assert( isObject());
  const TJsonParser::TObj *v = object.get(key);
  if (v == NULL || !v->isLiteral() || v->literal.type == LITERAL_NULL || v->literal.type == LITERAL_BOOL_FALSE || v->literal.type == LITERAL_BOOL_TRUE)
    return def_value;
  return (float)atof(v->literal.text);
}

int TJsonParser::TObj::getInt(const char *key, int def_value) const {
  assert(this);
  assert(isObject());
  const TJsonParser::TObj *v = object.get(key);
  if (v == NULL || !v->isLiteral() || v->literal.type == LITERAL_NULL || v->literal.type == LITERAL_BOOL_FALSE || v->literal.type == LITERAL_BOOL_TRUE)
    return def_value;
  return atol(v->literal.text);
}

bool TJsonParser::TObj::getBool(const char *key, bool def_value) const {
  assert(this);
  assert(isObject());
  const TJsonParser::TObj *v = object.get(key);
  if (v == NULL || !v->isLiteral())
    return def_value;
  if (v->literal.type == LITERAL_BOOL_FALSE)
    return false;
  if (v->literal.type == LITERAL_BOOL_TRUE)
    return true;
  // Only the native true/false are interpreted!
  return def_value;
}

const char *TJsonParser::TObj::getStr(const char *key) const {
  assert(this);
  assert(isObject());
  const TJsonParser::TObj *v = object.get(key);
  if (v == NULL || !v->isLiteral() || v->literal.type != LITERAL_STRING)
    return NULL;
  return v->literal.text;
}

