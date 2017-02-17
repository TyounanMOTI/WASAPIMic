using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using WASAPIMic;

public class MicLoopback : MonoBehaviour {
	Library.DebugLogDelegate debugLogFunc = message => Debug.Log(message);

	// Use this for initialization
	void Start () {
		Library.SetDebugLogFunc(debugLogFunc);
		Library.Initialize();
		Library.StartLoopback();
	}
}
