using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class HeaderFile
    {
        public int FileID { get; }
        public string FilePath { get; }
        public string RelativeFilePath { get; set; } = String.Empty;

        public List<HeaderFile> IncludedHeaders { get; } = new List<HeaderFile>();
        private readonly List<HeaderFile> _referencedHeaders  = new List<HeaderFile>();
        public IEnumerable<HeaderFile> ReferencedHeadersNoLock => _referencedHeaders;
        public IEnumerable<HeaderFile> ReferencedHeadersLocked
        {
            get
            {
                lock (_referencedHeaders)
                {
                    foreach (HeaderFile reference in _referencedHeaders)
                    {
                        yield return reference;
                    }
                }
            }
        }

        public FileParsingResult FileParsingResult { get; } = new FileParsingResult();

        public HeaderFile(Process process, string filepath)
        {
            FileID = process.HeaderFileIDGenerator.GenerateID();
            FilePath = filepath;
        }

        public void AddReferencedHeader(HeaderFile headerFile, bool isIncludedFile)
        {
            lock (_referencedHeaders)
            {
                if (_referencedHeaders.Contains(headerFile))
                    return;

                _referencedHeaders.Add(headerFile);

                if (isIncludedFile)
                    IncludedHeaders.Add(headerFile);
            }
        }
    }
}
