// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

/******************************************************************************
 * This file is auto-generated from a template file by the GenerateTests.csx  *
 * script in tests\src\JIT\HardwareIntrinsics\X86\Shared. In order to make    *
 * changes, please update the corresponding template and run according to the *
 * directions listed in the file.                                             *
 ******************************************************************************/

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Intrinsics;
using System.Runtime.Intrinsics.X86;

namespace JIT.HardwareIntrinsics.X86
{
    public static partial class Program
    {
        private static void SignInt32()
        {
            var test = new SimpleBinaryOpTest__SignInt32();

            if (test.IsSupported)
            {
                // Validates basic functionality works, using Unsafe.Read
                test.RunBasicScenario_UnsafeRead();

                if (Sse2.IsSupported)
                {
                    // Validates basic functionality works, using Load
                    test.RunBasicScenario_Load();

                    // Validates basic functionality works, using LoadAligned
                    test.RunBasicScenario_LoadAligned();
                }

                // Validates calling via reflection works, using Unsafe.Read
                test.RunReflectionScenario_UnsafeRead();

                if (Sse2.IsSupported)
                {
                    // Validates calling via reflection works, using Load
                    test.RunReflectionScenario_Load();

                    // Validates calling via reflection works, using LoadAligned
                    test.RunReflectionScenario_LoadAligned();
                }

                // Validates passing a static member works
                test.RunClsVarScenario();

                // Validates passing a local works, using Unsafe.Read
                test.RunLclVarScenario_UnsafeRead();

                if (Sse2.IsSupported)
                {
                    // Validates passing a local works, using Load
                    test.RunLclVarScenario_Load();

                    // Validates passing a local works, using LoadAligned
                    test.RunLclVarScenario_LoadAligned();
                }

                // Validates passing the field of a local works
                test.RunLclFldScenario();

                // Validates passing an instance member works
                test.RunFldScenario();
            }
            else
            {
                // Validates we throw on unsupported hardware
                test.RunUnsupportedScenario();
            }

            if (!test.Succeeded)
            {
                throw new Exception("One or more scenarios did not complete as expected.");
            }
        }
    }

    public sealed unsafe class SimpleBinaryOpTest__SignInt32
    {
        private static readonly int LargestVectorSize = 16;

        private static readonly int Op1ElementCount = Unsafe.SizeOf<Vector128<Int32>>() / sizeof(Int32);
        private static readonly int Op2ElementCount = Unsafe.SizeOf<Vector128<Int32>>() / sizeof(Int32);
        private static readonly int RetElementCount = Unsafe.SizeOf<Vector128<Int32>>() / sizeof(Int32);

        private static Int32[] _data1 = new Int32[Op1ElementCount];
        private static Int32[] _data2 = new Int32[Op2ElementCount];

        private static Vector128<Int32> _clsVar1;
        private static Vector128<Int32> _clsVar2;

        private Vector128<Int32> _fld1;
        private Vector128<Int32> _fld2;

        private SimpleBinaryOpTest__DataTable<Int32, Int32, Int32> _dataTable;

        static SimpleBinaryOpTest__SignInt32()
        {
            var random = new Random();

            for (var i = 0; i < Op1ElementCount; i++) { _data1[i] = (int)(random.Next(int.MinValue + 1, int.MaxValue)); }
            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Vector128<Int32>, byte>(ref _clsVar1), ref Unsafe.As<Int32, byte>(ref _data1[0]), (uint)Unsafe.SizeOf<Vector128<Int32>>());
            for (var i = 0; i < Op2ElementCount; i++) { _data2[i] = (int)(random.Next(int.MinValue, int.MaxValue)); }
            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Vector128<Int32>, byte>(ref _clsVar2), ref Unsafe.As<Int32, byte>(ref _data2[0]), (uint)Unsafe.SizeOf<Vector128<Int32>>());
        }

        public SimpleBinaryOpTest__SignInt32()
        {
            Succeeded = true;

            var random = new Random();

            for (var i = 0; i < Op1ElementCount; i++) { _data1[i] = (int)(random.Next(int.MinValue + 1, int.MaxValue)); }
            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Vector128<Int32>, byte>(ref _fld1), ref Unsafe.As<Int32, byte>(ref _data1[0]), (uint)Unsafe.SizeOf<Vector128<Int32>>());
            for (var i = 0; i < Op2ElementCount; i++) { _data2[i] = (int)(random.Next(int.MinValue, int.MaxValue)); }
            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Vector128<Int32>, byte>(ref _fld2), ref Unsafe.As<Int32, byte>(ref _data2[0]), (uint)Unsafe.SizeOf<Vector128<Int32>>());

            for (var i = 0; i < Op1ElementCount; i++) { _data1[i] = (int)(random.Next(int.MinValue + 1, int.MaxValue)); }
            for (var i = 0; i < Op2ElementCount; i++) { _data2[i] = (int)(random.Next(int.MinValue, int.MaxValue)); }
            _dataTable = new SimpleBinaryOpTest__DataTable<Int32, Int32, Int32>(_data1, _data2, new Int32[RetElementCount], LargestVectorSize);
        }

        public bool IsSupported => Ssse3.IsSupported;

        public bool Succeeded { get; set; }

        public void RunBasicScenario_UnsafeRead()
        {
            var result = Ssse3.Sign(
                Unsafe.Read<Vector128<Int32>>(_dataTable.inArray1Ptr),
                Unsafe.Read<Vector128<Int32>>(_dataTable.inArray2Ptr)
            );

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(_dataTable.inArray1Ptr, _dataTable.inArray2Ptr, _dataTable.outArrayPtr);
        }

        public void RunBasicScenario_Load()
        {
            var result = Ssse3.Sign(
                Sse2.LoadVector128((Int32*)(_dataTable.inArray1Ptr)),
                Sse2.LoadVector128((Int32*)(_dataTable.inArray2Ptr))
            );

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(_dataTable.inArray1Ptr, _dataTable.inArray2Ptr, _dataTable.outArrayPtr);
        }

        public void RunBasicScenario_LoadAligned()
        {
            var result = Ssse3.Sign(
                Sse2.LoadAlignedVector128((Int32*)(_dataTable.inArray1Ptr)),
                Sse2.LoadAlignedVector128((Int32*)(_dataTable.inArray2Ptr))
            );

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(_dataTable.inArray1Ptr, _dataTable.inArray2Ptr, _dataTable.outArrayPtr);
        }

        public void RunReflectionScenario_UnsafeRead()
        {
            var result = typeof(Ssse3).GetMethod(nameof(Ssse3.Sign), new Type[] { typeof(Vector128<Int32>), typeof(Vector128<Int32>) })
                                     .Invoke(null, new object[] {
                                        Unsafe.Read<Vector128<Int32>>(_dataTable.inArray1Ptr),
                                        Unsafe.Read<Vector128<Int32>>(_dataTable.inArray2Ptr)
                                     });

            Unsafe.Write(_dataTable.outArrayPtr, (Vector128<Int32>)(result));
            ValidateResult(_dataTable.inArray1Ptr, _dataTable.inArray2Ptr, _dataTable.outArrayPtr);
        }

        public void RunReflectionScenario_Load()
        {
            var result = typeof(Ssse3).GetMethod(nameof(Ssse3.Sign), new Type[] { typeof(Vector128<Int32>), typeof(Vector128<Int32>) })
                                     .Invoke(null, new object[] {
                                        Sse2.LoadVector128((Int32*)(_dataTable.inArray1Ptr)),
                                        Sse2.LoadVector128((Int32*)(_dataTable.inArray2Ptr))
                                     });

            Unsafe.Write(_dataTable.outArrayPtr, (Vector128<Int32>)(result));
            ValidateResult(_dataTable.inArray1Ptr, _dataTable.inArray2Ptr, _dataTable.outArrayPtr);
        }

        public void RunReflectionScenario_LoadAligned()
        {
            var result = typeof(Ssse3).GetMethod(nameof(Ssse3.Sign), new Type[] { typeof(Vector128<Int32>), typeof(Vector128<Int32>) })
                                     .Invoke(null, new object[] {
                                        Sse2.LoadAlignedVector128((Int32*)(_dataTable.inArray1Ptr)),
                                        Sse2.LoadAlignedVector128((Int32*)(_dataTable.inArray2Ptr))
                                     });

            Unsafe.Write(_dataTable.outArrayPtr, (Vector128<Int32>)(result));
            ValidateResult(_dataTable.inArray1Ptr, _dataTable.inArray2Ptr, _dataTable.outArrayPtr);
        }

        public void RunClsVarScenario()
        {
            var result = Ssse3.Sign(
                _clsVar1,
                _clsVar2
            );

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(_clsVar1, _clsVar2, _dataTable.outArrayPtr);
        }

        public void RunLclVarScenario_UnsafeRead()
        {
            var left = Unsafe.Read<Vector128<Int32>>(_dataTable.inArray1Ptr);
            var right = Unsafe.Read<Vector128<Int32>>(_dataTable.inArray2Ptr);
            var result = Ssse3.Sign(left, right);

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(left, right, _dataTable.outArrayPtr);
        }

        public void RunLclVarScenario_Load()
        {
            var left = Sse2.LoadVector128((Int32*)(_dataTable.inArray1Ptr));
            var right = Sse2.LoadVector128((Int32*)(_dataTable.inArray2Ptr));
            var result = Ssse3.Sign(left, right);

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(left, right, _dataTable.outArrayPtr);
        }

        public void RunLclVarScenario_LoadAligned()
        {
            var left = Sse2.LoadAlignedVector128((Int32*)(_dataTable.inArray1Ptr));
            var right = Sse2.LoadAlignedVector128((Int32*)(_dataTable.inArray2Ptr));
            var result = Ssse3.Sign(left, right);

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(left, right, _dataTable.outArrayPtr);
        }

        public void RunLclFldScenario()
        {
            var test = new SimpleBinaryOpTest__SignInt32();
            var result = Ssse3.Sign(test._fld1, test._fld2);

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(test._fld1, test._fld2, _dataTable.outArrayPtr);
        }

        public void RunFldScenario()
        {
            var result = Ssse3.Sign(_fld1, _fld2);

            Unsafe.Write(_dataTable.outArrayPtr, result);
            ValidateResult(_fld1, _fld2, _dataTable.outArrayPtr);
        }

        public void RunUnsupportedScenario()
        {
            Succeeded = false;

            try
            {
                RunBasicScenario_UnsafeRead();
            }
            catch (PlatformNotSupportedException)
            {
                Succeeded = true;
            }
        }

        private void ValidateResult(Vector128<Int32> left, Vector128<Int32> right, void* result, [CallerMemberName] string method = "")
        {
            Int32[] inArray1 = new Int32[Op1ElementCount];
            Int32[] inArray2 = new Int32[Op2ElementCount];
            Int32[] outArray = new Int32[RetElementCount];

            Unsafe.WriteUnaligned(ref Unsafe.As<Int32, byte>(ref inArray1[0]), left);
            Unsafe.WriteUnaligned(ref Unsafe.As<Int32, byte>(ref inArray2[0]), right);
            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Int32, byte>(ref outArray[0]), ref Unsafe.AsRef<byte>(result), (uint)Unsafe.SizeOf<Vector128<Int32>>());

            ValidateResult(inArray1, inArray2, outArray, method);
        }

        private void ValidateResult(void* left, void* right, void* result, [CallerMemberName] string method = "")
        {
            Int32[] inArray1 = new Int32[Op1ElementCount];
            Int32[] inArray2 = new Int32[Op2ElementCount];
            Int32[] outArray = new Int32[RetElementCount];

            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Int32, byte>(ref inArray1[0]), ref Unsafe.AsRef<byte>(left), (uint)Unsafe.SizeOf<Vector128<Int32>>());
            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Int32, byte>(ref inArray2[0]), ref Unsafe.AsRef<byte>(right), (uint)Unsafe.SizeOf<Vector128<Int32>>());
            Unsafe.CopyBlockUnaligned(ref Unsafe.As<Int32, byte>(ref outArray[0]), ref Unsafe.AsRef<byte>(result), (uint)Unsafe.SizeOf<Vector128<Int32>>());

            ValidateResult(inArray1, inArray2, outArray, method);
        }

        private void ValidateResult(Int32[] left, Int32[] right, Int32[] result, [CallerMemberName] string method = "")
        {
            if (result[0] != (right[0] < 0 ? (int)(-left[0]) : (right[0] > 0 ? left[0] : 0)))
            {
                Succeeded = false;
            }
            else
            {
                for (var i = 1; i < RetElementCount; i++)
                {
                    if (result[i] != (right[i] < 0 ? (int)(-left[i]) : (right[i] > 0 ? left[i] : 0)))
                    {
                        Succeeded = false;
                        break;
                    }
                }
            }

            if (!Succeeded)
            {
                Console.WriteLine($"{nameof(Ssse3)}.{nameof(Ssse3.Sign)}<Int32>(Vector128<Int32>, Vector128<Int32>): {method} failed:");
                Console.WriteLine($"    left: ({string.Join(", ", left)})");
                Console.WriteLine($"   right: ({string.Join(", ", right)})");
                Console.WriteLine($"  result: ({string.Join(", ", result)})");
                Console.WriteLine();
            }
        }
    }
}
