using ClangSharp;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class AttributeParser
    {
        private ParsingSettings? _parsingSettings = null;
        private char[] _relevantMainAttributeParsing = new char[2];
        private char[] _relevateSubAttributeParsing = new char[2];
        private List<List<string>> _splitAttributes = new List<List<string>>();
        private string _lastErrorMsg = string.Empty;
        public string LastErrorMsg => _lastErrorMsg;

        public void Setup(ParsingSettings parsingSettings)
        {
            _parsingSettings = parsingSettings;

            _relevantMainAttributeParsing = new char[2]
            {
                parsingSettings.PropertySeparator,      // ','
                parsingSettings.ArgumentEnclosers[0]    // '('
            };
            _relevateSubAttributeParsing = new char[2]
            {
                parsingSettings.ArgumentSeparator,      // ','
                parsingSettings.ArgumentEnclosers[1]    // ')'
            };
        }

        public void Clean()
        { 
            _splitAttributes.Clear();
            _lastErrorMsg = string.Empty;
        }

        private bool GetNextSubAttributes(ref string attributeString, ref bool bParsingSubArgs)
        {
            if (_splitAttributes.Count <= 0)
                return false;

            string nextSubAttribute = string.Empty;
            int idx = attributeString.IndexOfAny(_relevateSubAttributeParsing);
            if (idx < 0)
            {
                _lastErrorMsg = "Subproperty end encloser \"" + _parsingSettings.ArgumentEnclosers[1] + "\" is missing.";
                return false;
            }
            else if (attributeString[idx] == _parsingSettings.ArgumentSeparator)
            {
                nextSubAttribute = attributeString.Substring(0, idx);
                attributeString = attributeString.Substring(idx + 1);
            }
            else
            {
                // End subAttributes parsing
                bParsingSubArgs = false;
                nextSubAttribute = attributeString.Substring(0, idx);

                if (idx != attributeString.Length - 1)
                {
                    // The next character must is PropertySeparator
                    int separatorIdx = idx + 1;
                    while (separatorIdx < attributeString.Length && attributeString[separatorIdx] == ' ') separatorIdx++;
                    if (separatorIdx == attributeString.Length)
                    {
                        attributeString = String.Empty;
                    }
                    else if (attributeString[separatorIdx] != _parsingSettings.PropertySeparator)
                    {
                        _lastErrorMsg = "Property separator \"" + _parsingSettings.PropertySeparator + "\" is missing between two properties.";
                        return false;
                    }
                    else
                    {
                        attributeString = attributeString.Substring(separatorIdx + 1);
                    }
                }
                else
                {
                    attributeString = String.Empty;
                }
            }

            // Trim attribute string
            nextSubAttribute = nextSubAttribute.TrimStart();
            nextSubAttribute = nextSubAttribute.TrimEnd();

            if (nextSubAttribute.Length > 0)
                _splitAttributes[_splitAttributes.Count - 1].Add(nextSubAttribute);

            return true;
        }

        private bool GetNextAttribute(ref string attributeString, ref bool bParsingSubArgs)
        {
            string nextAttribute = string.Empty;
            int idx = attributeString.IndexOfAny(_relevantMainAttributeParsing);
            if (idx < 0)
            {
                nextAttribute = attributeString;
                attributeString = string.Empty;
            }
            else if (attributeString[idx] == _parsingSettings.PropertySeparator)
            {
                nextAttribute = attributeString.Substring(0, idx);
                attributeString = attributeString.Substring(idx + 1);
            }
            else
            {
                // Start subAttributes parsing
                bParsingSubArgs = true;
                nextAttribute = attributeString.Substring(0, idx);
                attributeString = attributeString.Substring(idx + 1);
            }

            // Trim attribute string
            nextAttribute = nextAttribute.TrimStart();
            nextAttribute = nextAttribute.TrimEnd();

            if (nextAttribute.Length > 0)
                _splitAttributes.Add(new List<string>() { nextAttribute });

            return true;
        } 

        private List<AttributeInfo>? GetAttriuteInfos(string annotateMsg, string attributeID)
        {
            if (_parsingSettings == null)
            {
                throw new Exception("Missing parsing settings of attribute parser.");
            }

            if (!string.Equals(annotateMsg.Substring(0, attributeID.Length), attributeID))
            {
                _lastErrorMsg = "The wrong macro has been used to attach properties to an entity.";
                return null;
            }

            // Clean up before parsing
            Clean();

            string attributesMsg = annotateMsg.Substring(attributeID.Length + 1);
            bool isValidAttributes = true;
            bool isParsingSubArgs = false;
            while (attributesMsg.Length > 0)
            {
                if (isParsingSubArgs)
                {
                    if (!GetNextSubAttributes(ref attributesMsg, ref isParsingSubArgs))
                    {
                        isValidAttributes = false;
                        break;
                    }
                }
                else if (!GetNextAttribute(ref attributesMsg, ref isParsingSubArgs))
                {
                    isValidAttributes = false;
                    break;
                }
            }

            if (isParsingSubArgs)
            {
                _lastErrorMsg = "Mainproperty end encloser \"" + _parsingSettings.ArgumentEnclosers[1] + "\" is missing.";
                isValidAttributes = false;
            }

            if (isValidAttributes)
            {
                List<AttributeInfo> attributes = new List<AttributeInfo>();
                FillAttributes(ref attributes);
                return attributes;
            }

            return null;
        }

        private void FillAttributes(ref List<AttributeInfo> attributeInfos)
        {
            foreach (var attrs in _splitAttributes)
            {
                AttributeInfo info = new AttributeInfo();
                info.Name = attrs[0];
                if (attrs.Count > 1)
                {
                    for (int i = 1; i < attrs.Count; i++)
                        info.Arguments.Add(attrs[i]);
                }

                attributeInfos.Add(info);
            }
        }

        public List<AttributeInfo>? GetClassAttributeInfos(string annotateMessage)
        {
            return GetAttriuteInfos(annotateMessage, "CLASS");
        }

        public List<AttributeInfo>? GetStructAttributeInfos(string annotateMessage)
        {
            return GetAttriuteInfos(annotateMessage, "STRUCT");
        }

        public List<AttributeInfo>? GetFieldAttributeInfos(string annotateMessage)
        {
            return GetAttriuteInfos(annotateMessage, "FIELD");
        }

        public List<AttributeInfo>? GetMethodAttributeInfos(string annotateMessage)
        {
            return GetAttriuteInfos(annotateMessage, "METHOD");
        }

        public List<AttributeInfo>? GetFunctionAttributeInfos(string annotateMessage)
        {
            return GetAttriuteInfos(annotateMessage, "FUNCTION");
        }

        public List<AttributeInfo>? GetEnumAttributeInfos(string annotateMessage)
        {
            return GetAttriuteInfos(annotateMessage, "ENUM");
        }

        public List<AttributeInfo>? GetEnumValueAttributeInfos(string annotateMessage)
        {
            return GetAttriuteInfos(annotateMessage, "ENUMVALUE");
        }
    }
}
