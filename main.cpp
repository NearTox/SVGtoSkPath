#include <iostream>
#include <string>

typedef float SkScalar;

struct SkPoint {

  SkScalar fX = 0;
  SkScalar fY = 0;

  const SkScalar& x() const { return fX; }
  const SkScalar& y() const { return fY; }

  void operator+=(const SkPoint &add) {
    fX += add.fX;
    fY += add.fY;
  }

  friend SkPoint operator+(const SkPoint &add, const SkPoint &add_2) {
    return SkPoint::Make(add.fX + add_2.fX, add.fY + add_2.fY);
  }

  static SkPoint Make(SkScalar x, SkScalar y) {
    SkPoint temp;
    temp.fX = x;
    temp.fY = y;
    return temp;
  }
  std::string to_string() const {
    return "SkPoint::Make(" +
      std::to_string(fX) + ", " +
      std::to_string(fY) + ")";
  }
};

struct SvgPathParser {
private:
  enum TOKEN : uint8_t {
    TOKEN_ABSOLUTE_COMMAND = 1,
    TOKEN_RELATIVE_COMMAND = 2,
    TOKEN_VALUE = 3,
    TOKEN_EOF = 4,
  };

  TOKEN mCurrentToken;
  size_t mIndex;
  std::string mPathString;

  TOKEN advanceToNextToken();

  char consumeCommand();

  SkPoint consumeAndTransformPoint();

  SkScalar consumeValue();
protected:
  virtual SkScalar transformX(SkScalar x) { return x; }
  virtual SkScalar transformY(SkScalar y) { return y; }

public:
  std::string parsePath(const std::string &s);
};

struct ConstrainedSvgPathParser : public SvgPathParser {
private:
  SkScalar originalWidth, originalHeight;
  SkScalar viewWidth, viewHeight;

protected:
  SkScalar transformX(SkScalar x) { return x * viewWidth / originalWidth; }
  SkScalar transformY(SkScalar y) { return y * viewHeight / originalHeight; }

public:

  ConstrainedSvgPathParser(SkScalar originalWidth, SkScalar originalHeight, SkScalar viewWidth,
    SkScalar viewHeight);
};
SvgPathParser::TOKEN SvgPathParser::advanceToNextToken() {
  while(mIndex < mPathString.size()) {
    char c = mPathString.at(mIndex);
    if('a' <= c && c <= 'z') {
      mCurrentToken = TOKEN_RELATIVE_COMMAND;
      return mCurrentToken;
    } else if('A' <= c && c <= 'Z') {
      mCurrentToken = TOKEN_ABSOLUTE_COMMAND;
      return mCurrentToken;
    } else if(('0' <= c && c <= '9') || c == '.' || c == '-') {
      mCurrentToken = TOKEN_VALUE;
      return mCurrentToken;
    }

    // skip unrecognized character
    ++mIndex;
  }
  mCurrentToken = TOKEN_EOF;
  return mCurrentToken;
}

char SvgPathParser::consumeCommand() {
  advanceToNextToken();
  if(mCurrentToken != TOKEN_RELATIVE_COMMAND && mCurrentToken != TOKEN_ABSOLUTE_COMMAND) {
    throw std::exception("Expected command", mIndex);
  }

  return mPathString.at(mIndex++);
}

SkPoint SvgPathParser::consumeAndTransformPoint() {
  SkScalar x = transformX(consumeValue());
  SkScalar y = transformY(consumeValue());
  return SkPoint::Make(x, y);
}

SkScalar SvgPathParser::consumeValue() {
  advanceToNextToken();
  if(mCurrentToken != TOKEN_VALUE) {
    throw std::exception("Expected value", mIndex);
  }

  bool start = true;
  bool seenDot = false;
  size_t index = mIndex;
  while(index < mPathString.size()) {
    char c = mPathString.at(index);
    if(!('0' <= c && c <= '9') && (c != '.' || seenDot) && (c != '-' || !start)) {
      // end of value
      break;
    }
    if(c == '.') {
      seenDot = true;
    }
    start = false;
    ++index;
  }

  if(index == mIndex) {
    throw std::exception("Expected value", mIndex);
  }

  std::string str = mPathString.substr(mIndex, index);
  try {
    SkScalar value = std::stof(str);
    mIndex = index;
    return value;
  } catch(std::invalid_argument e) {
    throw std::exception(("Invalid float value '" + str + "'.").c_str(), mIndex);
  }
}

std::string SvgPathParser::parsePath(const std::string &s) {
  /*SkPath path;
  if(SkParsePath::FromSVGString(s.c_str(), &path)) {
  return path;
  }*/
  mPathString = s;
  mIndex = 0;

  SkPoint ptX, ptX1, ptX0, ptX2;

  std::string p;
  p += "SkPath p;\n";
  p += "p.setFillType(SkPath::kWinding_FillType);\n";

  char lastcomand = 'z';
  while(mIndex < mPathString.size()) {
    char command = consumeCommand();
    bool relative = (mCurrentToken == TOKEN_RELATIVE_COMMAND);
    switch(command) {
      case 'M':
      case 'm':
      {
        // x
        // moveTo(x)
        bool firstPoint = true;
        while(advanceToNextToken() == TOKEN_VALUE) {
          SkPoint tempX = consumeAndTransformPoint();
          if(relative) {
            ptX += tempX;
          } else {
            ptX = tempX;
          }
          if(firstPoint) {
            p += "p.moveTo(" + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
            firstPoint = false;
            ptX0 = ptX;
          } else {
            p += "p.lineTo(" + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
          }
        }
        /*{
        SkPoint tempPoint;
        p.getLastPt(&tempPoint);
        assert(tempPoint == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'A':
      case 'a':
      {
        // rx ry x-axis-rotation large-arc-flag sweep-flag x y
        // addArc(oval, x2, sweep-flag)
        while(advanceToNextToken() == TOKEN_VALUE) {
          SkPoint tempR = consumeAndTransformPoint();
          SkScalar tempX_rot = consumeValue();
          SkScalar largeArc = consumeValue();
          SkScalar sweep = consumeValue();
          SkPoint tempX = consumeAndTransformPoint();
          if(relative) {
            ptX = ptX + tempX;
          } else {
            ptX = tempX;
          }
          p += "p.arcTo(" + tempR.to_string() + ", " + std::to_string(tempX_rot) + ", " +
            "(SkPath::ArcSize) SkToBool(" + std::to_string(largeArc) + "), " +
            "(SkPath::Direction) !SkToBool(" + std::to_string(sweep) + ")," + ptX.to_string() + ");\n";
        }
        /*{
        SkPoint tempPoint;
        p.getLastPt(&tempPoint);
        assert(tempPoint == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'S':
      case 's':
      {
        // x2, x
        // cubicTo(x1, x2, x)
        while(advanceToNextToken() == TOKEN_VALUE) {
          if(lastcomand == 's' || lastcomand == 'S' || lastcomand == 'c' || lastcomand == 'C') {
            ptX1.fX = 2 * ptX.x() - ptX2.x();
            ptX1.fY = 2 * ptX.y() - ptX2.y();
          } else {
            ptX1 = ptX;
          }
          SkPoint tempX2 = consumeAndTransformPoint();
          SkPoint tempX = consumeAndTransformPoint();
          if(relative) {
            ptX2 = ptX + tempX2;
            ptX = ptX + tempX;
          } else {
            ptX2 = tempX2;
            ptX = tempX;
          }
          p += "p.cubicTo(" + std::to_string(ptX1.x()) + ", " + std::to_string(ptX1.y()) + ", " + std::to_string(ptX2.x()) + ", " + std::to_string(ptX2.y()) + ", " + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'C':
      case 'c':
      {
        // curve command
        // x1, x2, x
        // cubicTo(x1, x2, x)
        while(advanceToNextToken() == TOKEN_VALUE) {
          SkPoint tempX1 = consumeAndTransformPoint();
          SkPoint tempX2 = consumeAndTransformPoint();
          SkPoint tempX = consumeAndTransformPoint();
          if(relative) {
            ptX1 = ptX + tempX1;
            ptX2 = ptX + tempX2;
            ptX = ptX + tempX;
          } else {
            ptX1 = tempX1;
            ptX2 = tempX2;
            ptX = tempX;
          }

          p += "p.cubicTo(" + std::to_string(ptX1.x()) + ", " + std::to_string(ptX1.y()) + ", " + std::to_string(ptX2.x()) + ", " + std::to_string(ptX2.y()) + ", " + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'T':
      case 't':
      {
        // x
        // quadTo(x1 x)
        while(advanceToNextToken() == TOKEN_VALUE) {
          if(lastcomand == 't' || lastcomand == 't' || lastcomand == 'q' || lastcomand == 'Q') {
            ptX1.fX = 2 * ptX.x() - ptX1.x();
            ptX1.fY = 2 * ptX.y() - ptX1.y();
          } else {
            ptX1 = ptX;
          }
          SkPoint tempX = consumeAndTransformPoint();
          if(relative) {
            ptX = ptX + tempX;
          } else {
            ptX = tempX;
          }
          p += "p.quadTo(" + std::to_string(ptX1.x()) + ", " + std::to_string(ptX1.y()) + ", " + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'Q':
      case 'q':
      {
        // x1 x
        // quadTo(x1 x)

        while(advanceToNextToken() == TOKEN_VALUE) {
          SkPoint tempX1 = consumeAndTransformPoint();
          SkPoint tempX = consumeAndTransformPoint();
          if(relative) {
            ptX1 = ptX + tempX1;
            ptX = ptX + tempX;
          } else {
            ptX1 = tempX1;
            ptX = tempX;
          }
          p += "p.quadTo(" + std::to_string(ptX1.x()) + ", " + std::to_string(ptX1.y()) + ", " + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'L':
      case 'l':
      {
        while(advanceToNextToken() == TOKEN_VALUE) {
          SkPoint tempX = consumeAndTransformPoint();
          if(relative) {
            ptX += tempX;
          } else {
            ptX = tempX;
          }
          p += "p.lineTo(" + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'H':
      case 'h':
      {
        while(advanceToNextToken() == TOKEN_VALUE) {
          SkScalar x = transformX(consumeValue());
          if(relative) {
            ptX.fX += x;
          } else {
            ptX.fX = x;
          }
          p += "p.lineTo(" + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'V':
      case 'v':
      {
        while(advanceToNextToken() == TOKEN_VALUE) {
          SkScalar y = transformY(consumeValue());
          if(relative) {
            ptX.fY += y;
          } else {
            ptX.fY = y;
          }
          p += "p.lineTo(" + std::to_string(ptX.x()) + ", " + std::to_string(ptX.y()) + ");\n";
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }

      case 'Z':
      case 'z':
      {
        // close command
        p += "p.close();\n";
        if(relative) {
          ptX = ptX0;
        }
        /*{
        SkPoint tempPoint1;
        p.getLastPt(&tempPoint1);
        assert(tempPoint1 == ptX);
        }*/
        lastcomand = command;
        break;
      }
    }
  }

  // TODO: I do clean mPathString after all?
  mPathString.clear();

  return p;
}

int main() {
  std::string path;
  std::string path_temp;
  std::string result;
  uint8_t doExit = false;
  do {
    std::getline(std::cin, path_temp);
    path += path_temp;
    if(path_temp.size() == 0) {
      if(path.size() != 0) {
        SvgPathParser temp;
        try {
          result += temp.parsePath(path);
        } catch(std::exception e) {
          std::cerr << e.what();
        }
        path.clear();
      }
      doExit++;
    } else {
      doExit = 0;
    }
  } while(doExit != 2);
  std::cout << result;
  system("pause");
  return 0;
}