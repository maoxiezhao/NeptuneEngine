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
    }
}
