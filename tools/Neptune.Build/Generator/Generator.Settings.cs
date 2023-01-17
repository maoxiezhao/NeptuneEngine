using Neptune.Gen;
using Nett;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Build.Generator
{
    public static partial class CodeGenerator
    {
        private const string _tomlSectionName = "ParsingSettings";
        private static Gen.ParsingSettings _parsingSettings = null;
        private static string GetParsingSettingsPath()
        {
            string currentPath = typeof(CodeGenManager).Assembly.Location;
            var currentDirectory = Path.GetDirectoryName(currentPath);
            return Path.Combine(currentDirectory, "Settings.toml");
        }

        private static Gen.ParsingSettings GetParsingSettings()
        {
            if (_parsingSettings != null)
                return _parsingSettings;

            var settingsPath = GetParsingSettingsPath();
            if (!File.Exists(settingsPath))
                return null;

            var toml = Toml.ReadFile(settingsPath);
            if (toml == null)
                return null;

            var settings = new Gen.ParsingSettings();
            if (!LoadFromTomlConfig(settings, toml))
                return null;

            _parsingSettings = settings;
            return _parsingSettings;
        }

        internal static bool LoadCppVersion(Gen.ParsingSettings settings, TomlTable tomlParsingSettings)
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

            settings.ECppVersion = version;
            return true;
        }

        internal static bool loadShouldParseAllEntities(Gen.ParsingSettings settings, TomlTable tomlParsingSettings)
        {
            settings.ShouldParseAllNamespaces = tomlParsingSettings.Get<bool>("shouldParseAllNamespaces");
            settings.ShouldParseAllClasses = tomlParsingSettings.Get<bool>("shouldParseAllClasses");
            settings.ShouldParseAllStructs = tomlParsingSettings.Get<bool>("shouldParseAllStructs");
            settings.ShouldParseAllVariables = tomlParsingSettings.Get<bool>("shouldParseAllVariables");
            settings.ShouldParseAllFields = tomlParsingSettings.Get<bool>("shouldParseAllFields");
            settings.ShouldParseAllFunctions = tomlParsingSettings.Get<bool>("shouldParseAllFunctions");
            settings.ShouldParseAllMethods = tomlParsingSettings.Get<bool>("shouldParseAllMethods");
            settings.ShouldParseAllEnums = tomlParsingSettings.Get<bool>("shouldParseAllEnums");
            settings.ShouldParseAllEnumValues = tomlParsingSettings.Get<bool>("shouldParseAllEnumValues");
            return true;
        }

        internal static bool LoadShouldParsingOnFirstError(Gen.ParsingSettings settings, TomlTable tomlParsingSettings)
        {
            settings.ShouldAbortParsingOnFirstError = tomlParsingSettings.Get<bool>("shouldAbortParsingOnFirstError");
            return true;
        }

        internal static bool LoadParsingProperty(Gen.ParsingSettings settings, TomlTable tomlParsingSettings)
        {
            settings.ApiTokens.Class = tomlParsingSettings.Get<string>("classMacroName");
            settings.ApiTokens.Struct = tomlParsingSettings.Get<string>("structMacroName");
            settings.ApiTokens.Variable = tomlParsingSettings.Get<string>("variableMacroName");
            settings.ApiTokens.Field = tomlParsingSettings.Get<string>("fieldMacroName");
            settings.ApiTokens.Function = tomlParsingSettings.Get<string>("functionMacroName");
            settings.ApiTokens.Method = tomlParsingSettings.Get<string>("methodMacroName");
            settings.ApiTokens.Enum = tomlParsingSettings.Get<string>("enumMacroName");
            settings.ApiTokens.EnumValue = tomlParsingSettings.Get<string>("enumValueMacroName");

            settings.PropertySeparator = tomlParsingSettings.Get<string>("propertySeparator")[0];
            settings.ArgumentSeparator = tomlParsingSettings.Get<string>("argumentSeparator")[0];
            settings.ArgumentEnclosers[0] = tomlParsingSettings.Get<string>("argumentStartEncloser")[0];
            settings.ArgumentEnclosers[1] = tomlParsingSettings.Get<string>("argumentEndEncloser")[0];
            return true;
        }

        internal static bool LoadFromTomlConfig(Gen.ParsingSettings settings, TomlTable tomlTable)
        {
            var tomlParsingSettings = tomlTable.Get<TomlTable>(_tomlSectionName);
            if (tomlParsingSettings == null)
                return false;

            if (!LoadCppVersion(settings, tomlParsingSettings)) return false;
            if (!loadShouldParseAllEntities(settings, tomlParsingSettings)) return false;
            if (!LoadShouldParsingOnFirstError(settings, tomlParsingSettings)) return false;
            if (!LoadParsingProperty(settings, tomlParsingSettings)) return false;

            return true;
        }
    }
}
