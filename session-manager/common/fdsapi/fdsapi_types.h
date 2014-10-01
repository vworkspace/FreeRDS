/**
 * Autogenerated by Thrift Compiler (0.9.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef fdsapi_TYPES_H
#define fdsapi_TYPES_H

#include <thrift/Thrift.h>
#include <thrift/TApplicationException.h>
#include <thrift/protocol/TProtocol.h>
#include <thrift/transport/TTransport.h>



namespace freerds {

typedef bool TBOOL;

typedef int16_t TINT16;

typedef int32_t TINT32;

typedef std::string TSTRING;

typedef std::vector<class TSessionInfo>  TSessionInfoList;

typedef struct _TClientDisplay__isset {
  _TClientDisplay__isset() : displayWidth(false), displayHeight(false), colorDepth(false) {}
  bool displayWidth;
  bool displayHeight;
  bool colorDepth;
} _TClientDisplay__isset;

class TClientDisplay {
 public:

  static const char* ascii_fingerprint; // = "6435B39C87AB0E30F30BEDEFD7328C0D";
  static const uint8_t binary_fingerprint[16]; // = {0x64,0x35,0xB3,0x9C,0x87,0xAB,0x0E,0x30,0xF3,0x0B,0xED,0xEF,0xD7,0x32,0x8C,0x0D};

  TClientDisplay() : displayWidth(0), displayHeight(0), colorDepth(0) {
  }

  virtual ~TClientDisplay() throw() {}

  TINT32 displayWidth;
  TINT32 displayHeight;
  TINT32 colorDepth;

  _TClientDisplay__isset __isset;

  void __set_displayWidth(const TINT32 val) {
    displayWidth = val;
  }

  void __set_displayHeight(const TINT32 val) {
    displayHeight = val;
  }

  void __set_colorDepth(const TINT32 val) {
    colorDepth = val;
  }

  bool operator == (const TClientDisplay & rhs) const
  {
    if (!(displayWidth == rhs.displayWidth))
      return false;
    if (!(displayHeight == rhs.displayHeight))
      return false;
    if (!(colorDepth == rhs.colorDepth))
      return false;
    return true;
  }
  bool operator != (const TClientDisplay &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TClientDisplay & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

void swap(TClientDisplay &a, TClientDisplay &b);

typedef struct _TSessionInfo__isset {
  _TSessionInfo__isset() : sessionId(false), connectState(false), winStationName(false) {}
  bool sessionId;
  bool connectState;
  bool winStationName;
} _TSessionInfo__isset;

class TSessionInfo {
 public:

  static const char* ascii_fingerprint; // = "4086F12A5C2D615560236565C542F3C3";
  static const uint8_t binary_fingerprint[16]; // = {0x40,0x86,0xF1,0x2A,0x5C,0x2D,0x61,0x55,0x60,0x23,0x65,0x65,0xC5,0x42,0xF3,0xC3};

  TSessionInfo() : sessionId(0), connectState(0), winStationName() {
  }

  virtual ~TSessionInfo() throw() {}

  TINT32 sessionId;
  TINT32 connectState;
  TSTRING winStationName;

  _TSessionInfo__isset __isset;

  void __set_sessionId(const TINT32 val) {
    sessionId = val;
  }

  void __set_connectState(const TINT32 val) {
    connectState = val;
  }

  void __set_winStationName(const TSTRING& val) {
    winStationName = val;
  }

  bool operator == (const TSessionInfo & rhs) const
  {
    if (!(sessionId == rhs.sessionId))
      return false;
    if (!(connectState == rhs.connectState))
      return false;
    if (!(winStationName == rhs.winStationName))
      return false;
    return true;
  }
  bool operator != (const TSessionInfo &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TSessionInfo & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

void swap(TSessionInfo &a, TSessionInfo &b);

typedef struct _TSessionInfoValue__isset {
  _TSessionInfoValue__isset() : boolValue(false), int16Value(false), int32Value(false), stringValue(false), displayValue(false) {}
  bool boolValue;
  bool int16Value;
  bool int32Value;
  bool stringValue;
  bool displayValue;
} _TSessionInfoValue__isset;

class TSessionInfoValue {
 public:

  static const char* ascii_fingerprint; // = "7EBBEEF91A8512A99B13F41EFCC46DD3";
  static const uint8_t binary_fingerprint[16]; // = {0x7E,0xBB,0xEE,0xF9,0x1A,0x85,0x12,0xA9,0x9B,0x13,0xF4,0x1E,0xFC,0xC4,0x6D,0xD3};

  TSessionInfoValue() : boolValue(0), int16Value(0), int32Value(0), stringValue() {
  }

  virtual ~TSessionInfoValue() throw() {}

  TBOOL boolValue;
  TINT16 int16Value;
  TINT32 int32Value;
  TSTRING stringValue;
  TClientDisplay displayValue;

  _TSessionInfoValue__isset __isset;

  void __set_boolValue(const TBOOL val) {
    boolValue = val;
  }

  void __set_int16Value(const TINT16 val) {
    int16Value = val;
  }

  void __set_int32Value(const TINT32 val) {
    int32Value = val;
  }

  void __set_stringValue(const TSTRING& val) {
    stringValue = val;
  }

  void __set_displayValue(const TClientDisplay& val) {
    displayValue = val;
  }

  bool operator == (const TSessionInfoValue & rhs) const
  {
    if (!(boolValue == rhs.boolValue))
      return false;
    if (!(int16Value == rhs.int16Value))
      return false;
    if (!(int32Value == rhs.int32Value))
      return false;
    if (!(stringValue == rhs.stringValue))
      return false;
    if (!(displayValue == rhs.displayValue))
      return false;
    return true;
  }
  bool operator != (const TSessionInfoValue &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TSessionInfoValue & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

void swap(TSessionInfoValue &a, TSessionInfoValue &b);

typedef struct _TReturnEnumerateSessions__isset {
  _TReturnEnumerateSessions__isset() : returnValue(false), sessionInfoList(false) {}
  bool returnValue;
  bool sessionInfoList;
} _TReturnEnumerateSessions__isset;

class TReturnEnumerateSessions {
 public:

  static const char* ascii_fingerprint; // = "5E1654B463B78DBF34D5E70CA455347C";
  static const uint8_t binary_fingerprint[16]; // = {0x5E,0x16,0x54,0xB4,0x63,0xB7,0x8D,0xBF,0x34,0xD5,0xE7,0x0C,0xA4,0x55,0x34,0x7C};

  TReturnEnumerateSessions() : returnValue(0) {
  }

  virtual ~TReturnEnumerateSessions() throw() {}

  TBOOL returnValue;
  TSessionInfoList sessionInfoList;

  _TReturnEnumerateSessions__isset __isset;

  void __set_returnValue(const TBOOL val) {
    returnValue = val;
  }

  void __set_sessionInfoList(const TSessionInfoList& val) {
    sessionInfoList = val;
  }

  bool operator == (const TReturnEnumerateSessions & rhs) const
  {
    if (!(returnValue == rhs.returnValue))
      return false;
    if (!(sessionInfoList == rhs.sessionInfoList))
      return false;
    return true;
  }
  bool operator != (const TReturnEnumerateSessions &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TReturnEnumerateSessions & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

void swap(TReturnEnumerateSessions &a, TReturnEnumerateSessions &b);

typedef struct _TReturnQuerySessionInformation__isset {
  _TReturnQuerySessionInformation__isset() : returnValue(false), infoValue(false) {}
  bool returnValue;
  bool infoValue;
} _TReturnQuerySessionInformation__isset;

class TReturnQuerySessionInformation {
 public:

  static const char* ascii_fingerprint; // = "F29EFD14B0795544EEB6D15C1CC61C45";
  static const uint8_t binary_fingerprint[16]; // = {0xF2,0x9E,0xFD,0x14,0xB0,0x79,0x55,0x44,0xEE,0xB6,0xD1,0x5C,0x1C,0xC6,0x1C,0x45};

  TReturnQuerySessionInformation() : returnValue(0) {
  }

  virtual ~TReturnQuerySessionInformation() throw() {}

  TBOOL returnValue;
  TSessionInfoValue infoValue;

  _TReturnQuerySessionInformation__isset __isset;

  void __set_returnValue(const TBOOL val) {
    returnValue = val;
  }

  void __set_infoValue(const TSessionInfoValue& val) {
    infoValue = val;
  }

  bool operator == (const TReturnQuerySessionInformation & rhs) const
  {
    if (!(returnValue == rhs.returnValue))
      return false;
    if (!(infoValue == rhs.infoValue))
      return false;
    return true;
  }
  bool operator != (const TReturnQuerySessionInformation &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TReturnQuerySessionInformation & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

void swap(TReturnQuerySessionInformation &a, TReturnQuerySessionInformation &b);

} // namespace

#endif
