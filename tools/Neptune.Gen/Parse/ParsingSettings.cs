using Nett;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public enum ECppVersion
    {
        Unknown,
        Cpp17 = 17,
		Cpp20 = 20
	};

    public class ParsingSettings
    {
        public ApiTokens ApiTokens { get; set; } = new ApiTokens();
        public ECppVersion ECppVersion { get; set; } = ECppVersion.Cpp17;

        /** Char used to separate two properties. */
        public char PropertySeparator = ',';

        /** Char used to separate two sub properties. */
        public char ArgumentSeparator = ',';

        /** Chars used to respectively start and close a group of sub properties. */
        public char[] ArgumentEnclosers = new char[2]{ '(', ')' };

        public bool ShouldParseAllNamespaces = false;
        public bool ShouldParseAllClasses = false;
        public bool ShouldParseAllStructs = false;
        public bool ShouldParseAllVariables = false;
        public bool ShouldParseAllFields = false;
        public bool ShouldParseAllFunctions = false;
        public bool ShouldParseAllMethods = false;
        public bool ShouldParseAllEnums = false;
        public bool ShouldParseAllEnumValues = true;

        // Should parsing be aborted when an error is encountered or not.
        public bool ShouldAbortParsingOnFirstError = true;

        private const string _tomlSectionName = "ParsingSettings";
        private bool LoadCppVersion(TomlTable tomlParsingSettings)
        {
            int loadedCppVersion = tomlParsingSettings.Get<int>("cppVersion");
            ECppVersion version = ECppVersion.Unknown;
            switch (loadedCppVersion)
            {
                case 17:
                    version = ECppVersion.Cpp17;
                    break;
                case 20:
                    version = ECppVersion.Cpp20;
                    break;
            }

            if (version == ECppVersion.Unknown)
            {
                Log.Error("Unmatched cpp version.");
                return false;
            }

            ECppVersion = version;
            return true;
        }

        private bool loadShouldParseAllEntities(TomlTable tomlParsingSettings)
        {
            ShouldParseAllNamespaces = tomlParsingSettings.Get<bool>("shouldParseAllNamespaces");
            ShouldParseAllClasses = tomlParsingSettings.Get<bool>("shouldParseAllClasses");
            ShouldParseAllStructs = tomlParsingSettings.Get<bool>("shouldParseAllStructs");
            ShouldParseAllVariables = tomlParsingSettings.Get<bool>("shouldParseAllVariables");
            ShouldParseAllFields = tomlParsingSettings.Get<bool>("shouldParseAllFields");
            ShouldParseAllFunctions = tomlParsingSettings.Get<bool>("shouldParseAllFunctions");
            ShouldParseAllMethods = tomlParsingSettings.Get<bool>("shouldParseAllMethods");
            ShouldParseAllEnums = tomlParsingSettings.Get<bool>("shouldParseAllEnums");
            ShouldParseAllEnumValues = tomlParsingSettings.Get<bool>("shouldParseAllEnumValues");
            return true;
        }

        private bool LoadShouldParsingOnFirstError(TomlTable tomlParsingSettings)
        {
            ShouldAbortParsingOnFirstError = tomlParsingSettings.Get<bool>("shouldAbortParsingOnFirstError");
            return true;
        }

        private bool LoadParsingProperty(TomlTable tomlParsingSettings)
        {
            ApiTokens.Class = tomlParsingSettings.Get<string>("classMacroName");
            ApiTokens.Struct = tomlParsingSettings.Get<string>("structMacroName");
            ApiTokens.Variable = tomlParsingSettings.Get<string>("variableMacroName");
            ApiTokens.Field = tomlParsingSettings.Get<string>("fieldMacroName");
            ApiTokens.Function = tomlParsingSettings.Get<string>("functionMacroName");
            ApiTokens.Method = tomlParsingSettings.Get<string>("methodMacroName");
            ApiTokens.Enum = tomlParsingSettings.Get<string>("enumMacroName");
            ApiTokens.EnumValue = tomlParsingSettings.Get<string>("enumValueMacroName");

            PropertySeparator = tomlParsingSettings.Get<string>("propertySeparator")[0];
            ArgumentSeparator = tomlParsingSettings.Get<string>("argumentSeparator")[0];
            ArgumentEnclosers[0] = tomlParsingSettings.Get<string>("argumentStartEncloser")[0];
            ArgumentEnclosers[1] = tomlParsingSettings.Get<string>("argumentEndEncloser")[0];
            return true;
        }

        public bool LoadFromTomlConfig(TomlTable tomlTable)
        {
            var tomlParsingSettings = tomlTable.Get<TomlTable>(_tomlSectionName);
            if (tomlParsingSettings == null)
                return false;

            if (!LoadCppVersion(tomlParsingSettings)) return false;
            if (!loadShouldParseAllEntities(tomlParsingSettings)) return false;
            if (!LoadShouldParsingOnFirstError(tomlParsingSettings)) return false;
            if (!LoadParsingProperty(tomlParsingSettings)) return false;

            return true;
        }
    }
}
