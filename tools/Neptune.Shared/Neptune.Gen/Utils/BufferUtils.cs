using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Net.Sockets;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public class GenBuffer
    {
        public Memory<char> Memory { get; set; }
        public char[] Block;
        public GenBuffer? NextBuffer { get; set; } = null;
        public int Bucket { get; }

        public GenBuffer(int size, int bucket, int bucketSize)
        {
            Block = new char[bucketSize];
            Bucket = bucket;
            Reset(size);
        }

        public void Reset(int size)
        {
            Memory = new Memory<char>(Block, 0, size);
        }
    }

    public class GenBufferPool
    {
        private const int MinSize = 1024 * 16;
        private static readonly int BuckedAdjustment = BitOperations.Log2((uint)MinSize);
        private static readonly GenBuffer?[] BucketBuffers = new GenBuffer?[32 - BuckedAdjustment];

        public static GenBuffer Alloc(StringBuilder builder)
        {
            return Alloc(builder.Length);
        }

        public static GenBuffer Alloc(int size)
        {
            if (size <= MinSize)
            {
                return AllocInternal(size, 0, MinSize);
            }
            else
            {

                // Round up the size to the next larger power of two if it isn't a power of two
                uint usize = (uint)size;
                --usize;
                usize |= usize >> 1;
                usize |= usize >> 2;
                usize |= usize >> 4;
                usize |= usize >> 8;
                usize |= usize >> 16;
                ++usize;
                int bucket = BitOperations.Log2(usize) - BuckedAdjustment;
                return AllocInternal(size, bucket, (int)usize);
            }
        }

        public static void Free(GenBuffer buffer)
        {
            lock (BucketBuffers)
            {
                buffer.NextBuffer = BucketBuffers[buffer.Bucket];
                BucketBuffers[buffer.Bucket] = buffer;
            }
        }

        private static GenBuffer AllocInternal(int size, int bucket, int bucketSize)
        {
            lock (BucketBuffers)
            {
                if (BucketBuffers[bucket] != null)
                {
                    GenBuffer buffer = BucketBuffers[bucket]!;
                    BucketBuffers[bucket] = buffer.NextBuffer;
                    buffer.Reset(size);
                    return buffer;
                }
            }
            return new GenBuffer(size, bucket, bucketSize);
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
