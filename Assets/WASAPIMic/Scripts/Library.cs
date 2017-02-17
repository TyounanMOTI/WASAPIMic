using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.Runtime.InteropServices;

namespace WASAPIMic
{
	public class Library
	{
		public delegate void DebugLogDelegate([In, MarshalAs(UnmanagedType.LPWStr)]string message);

		[DllImport("WASAPIMic", CharSet = CharSet.Unicode)]
		public static extern void SetDebugLogFunc(DebugLogDelegate func);

		[DllImport("WASAPIMic")]
		public static extern void Initialize();

		[DllImport("WASAPIMic")]
		public static extern void StartLoopback();

		[DllImport("WASAPIMic")]
		public static extern void StopLoopback();
	}
}
