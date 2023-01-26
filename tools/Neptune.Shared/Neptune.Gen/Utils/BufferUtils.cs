using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class GenBuffer
    {
        public Memory<char> Memory { get; set; }
    }

    public class GenBufferPool
    {
        public static GenBuffer Alloc(StringBuilder builder)
        {
            return null;
        }

        public static void Free(GenBuffer buffer)
        {
        }
    }

    public class GenBufferHandle : IDisposable
    {
        public GenBuffer Buffer;

        public GenBufferHandle(StringBuilder builder)
        {
            Buffer = GenBufferPool.Alloc(builder);
        }

        public void Dispose()
        {
            GenBufferPool.Free(Buffer);
        }
    }
}
