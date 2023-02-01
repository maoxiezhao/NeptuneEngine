using ClangSharp;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public enum ETraversalBehaviour
    {
        /**
		*	Recursively traverse the entities contained in the current entity, using
		*	the same visitor and environment.
		*/
        Recurse = 0,

		/**
		*	Continues the entities traversal with the next sibling entity without visiting
		*	nested entities.
		*/
		Continue,

		/**
		*	Cancel the traversal on the siblings of the same entity type and resume
		*	it with the next sibling of a different type.
		*/
		Break,

		/**
		*	Abort the entity traversal but makes the generateCode method return true (success).
		*/
		AbortWithSuccess,

		/**
		*	Abort the entity traversal and make the generateCode method return false (failure).
		*/
		AbortWithFailure
    };

    public abstract class CppReflectGenerator
    {
        public const string GeneratedBodyMacroSuffix = "GENERATED_BODY";

        protected HeaderFile HeaderFile { get; }
        protected CodeGenCppReflectSettings Settings { get; }
        protected Manifest.ModuleInfo ModuleInfo => HeaderFile.ModuleInfo;
        protected string ModuleAPI => $"{ModuleInfo.ModuleName.ToUpper()}_API ";

        public struct HeaderInfo
        {
            public string IncludePath { get; set; }
            public string FileID { get; set; }
        }
        public readonly HeaderInfo CurrentHeaderInfo;
        public string HeaderFileID => CurrentHeaderInfo.FileID;

        public CppReflectGenerator(HeaderFile headerFile, CodeGenCppReflectSettings settings)
        {
            HeaderFile = headerFile;
            Settings = settings;

            char[] outFilePath = new char[HeaderFile.FilePath.Length + 4];
            outFilePath[0] = 'F';
            outFilePath[1] = 'I';
            outFilePath[2] = 'D';
            outFilePath[3] = '_';
            for (int index = 0; index < HeaderFile.FilePath.Length; ++index)
            {
                outFilePath[index + 4] = StringUtils.IsAlnum(HeaderFile.FilePath[index]) ? HeaderFile.FilePath[index] : '_';
            }
            CurrentHeaderInfo.FileID = new string(outFilePath);
            CurrentHeaderInfo.IncludePath = HeaderFile.FilePath;
        }

        public ETraversalBehaviour ForeachEntityPair(StringBuilder builder)
		{
			ETraversalBehaviour result = ETraversalBehaviour.Break;
			var parsingResult = HeaderFile.FileParsingResult;
			if (parsingResult == null || parsingResult.Errors.Count > 0)
				return ETraversalBehaviour.Break;

            foreach (var classInfo in parsingResult.Classes)
            {
                result = EntityForeachVisitInStruct(builder, classInfo);
                if (result == ETraversalBehaviour.Break)
                    break;
                else if (result == ETraversalBehaviour.AbortWithSuccess || result == ETraversalBehaviour.AbortWithFailure)
                    return result;
            }

            foreach (var structInfo in parsingResult.Structs)
            {
                result = EntityForeachVisitInStruct(builder, structInfo);
                if (result == ETraversalBehaviour.Break)
                    break;
                else if (result == ETraversalBehaviour.AbortWithSuccess || result == ETraversalBehaviour.AbortWithFailure)
                    return result;
            }

            foreach (var enumInfo in parsingResult.Enums)
			{
				result = EntityForeachVisitInEnum(builder, enumInfo);
                if (result == ETraversalBehaviour.Break)
					break;
				else if (result == ETraversalBehaviour.AbortWithSuccess || result == ETraversalBehaviour.AbortWithFailure)
					return result;
			}
			return result;
		}

		protected ETraversalBehaviour EntityForeachVisitInEnum(StringBuilder builder, EnumInfo enumInfo)
		{
            var ret = GenerateCodeForEntity(builder, enumInfo);
			if (ret != ETraversalBehaviour.Recurse)
				return ret;

            foreach (var enumValueInfo in enumInfo.EnumValues)
            {
                ret = GenerateCodeForEntity(builder, enumValueInfo);

				if (ret == ETraversalBehaviour.Break)
					break;
				else if (ret == ETraversalBehaviour.AbortWithSuccess || ret == ETraversalBehaviour.AbortWithFailure)
					return ret;
            }

			return ETraversalBehaviour.Recurse;
        }

        protected ETraversalBehaviour EntityForeachVisitInStruct(StringBuilder builder, StructClassInfo structClassInfo)
        {
            var ret = GenerateCodeForEntity(builder, structClassInfo);
            if (ret != ETraversalBehaviour.Recurse)
                return ret;

            return ETraversalBehaviour.Recurse;
        }

        protected abstract ETraversalBehaviour GenerateCodeForEntity(StringBuilder builder, EntityInfo entityInfo);

        public abstract void Generate(CodeGenFactory CodeGenFactory);
    }
}
