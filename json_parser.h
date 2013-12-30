#ifndef INC_JOHN_JSON_PARSER_H_
#define INC_JOHN_JSON_PARSER_H_

class TJsonParser {

public:

  enum TObjType {
    OBJ_MAP
  , OBJ_ARRAY
  , OBJ_LITERAL
  , OBJ_INVALID
  };

  enum TLiteralType {
    LITERAL_BOOL_TRUE
  , LITERAL_BOOL_FALSE
  , LITERAL_NULL
  , LITERAL_NUMERIC
  , LITERAL_STRING
  , LITERAL_INVALID
  };

  // -----------------------------------------
  TJsonParser( size_t buffer_size = 16 * 1024 );
  ~TJsonParser();
  const char *getErrorText() const { return error_text; }

  struct TObj {

    struct TLiteral {
      const char*  text;
      TLiteralType type;
    };

    struct TAttributes {
      const char **keys; // Array of keys 
      TObj **values;     // Array of values.
      size_t len;        // Number of key-value-pairs.
      const TObj *get(const char *akey) const;
    };

    struct TArray {
      TObj **values; // Array of values
      size_t len;    // Number of elements
      const TObj *at(size_t n) const;
    };

    TObjType      type;
    union {
      TLiteral    literal;
      TAttributes object;
      TArray      array;
    };
    TObj() : type(OBJ_INVALID) { }

    // Can be called even when this is null
    bool isArray() const { return this != NULL && type == OBJ_ARRAY; }
    bool isObject() const { return this != NULL && type == OBJ_MAP; }
    bool isLiteral() const { return this != NULL && type == OBJ_LITERAL; }

    // helpers, assume the obj to be of type obj_map or will return the def_value
    float       getFloat(const char *key, float def_value) const;
    int         getInt(const char *key, int def_value) const;
    bool        getBool(const char *key, bool def_value) const;
    const char *getStr(const char *key) const;
  };

  const TObj *parse(const char *buf, size_t nbytes);

private:

  enum TOpCode {
    MAP_OPEN
  , MAP_CLOSE
  , MAP_KEY_SEPARATOR
  , ARRAY_OPEN
  , ARRAY_CLOSE
  , COMMA_SEPARATOR
  , MAP_KEY
  , BOOL_TRUE_VALUE
  , BOOL_FALSE_VALUE
  , NULL_VALUE
  , NUMERIC_VALUE
  , STRING_VALUE
  , END_OF_STREAM
  , INVALID_OPCODE
  , NUM_OPCODES
  };

  TOpCode  scanOpCode();
  bool     checkKeyword(const char *word, size_t word_length);
  TObjType parseObj(TObj *obj);
  TObjType parseMap(TObj *obj);
  TObjType parseArray(TObj *obj);
  TObjType parseLiteral(TOpCode op, TObj *obj);

  const char *start;
  const char *end;
  const char *top;

  // -----------------------------------------
  // Used while parsing
  const char *last_str;

  // -----------------------------------------
  // Error 
  char error_text[256];
  void setErrorText(const char *new_error);

  // -----------------------------------------
  // Destination structured buffer
  typedef unsigned char u8;
  u8 *out_base;
  u8 *out_top;
  size_t out_buffer_size;
  static const int max_children_per_node = 64;
  size_t getUsedBytes() const { return ( out_top - out_base ); }
  void *allocData( size_t nbytes );
  TObj *allocObj();
  const char *allocStr(const char *buf, size_t nbytes);
};

#endif


