/*
 * 1. Download ScriptCommunicator
 * 2. Actions -> Scripts
 * 3. Script -> Add worker script (select this file)
 * 4. Press start
*/

var options = {
	agent: "Mega 2560",
	server: {
		ip: "192.168.46.9",
		port: 9001,
	},
	
	cases: [
		
		//
		// 1. Framing:
		//
			
		/* - 1.1 Text Messages */
		1, 2, 3, 4, 5, 6, 7, 8,
		/* - 1.2 Binary Messages: */ 
		9, 10, 11, 12, 13, 14, 15, 16,
		
		//
		// 2. Pings/Pongs:
		//
		
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
		
		//
		// 3. Reserved Bits:
		//
		
		28, 29, 30, 31, 32, 33, 34,
		
		//
		// 4. Opcodes
		//
		
		/* - 4.1 Non-control Opcodes */
		35, 36, 37, 38, 39,
		/* - 4.2 Control Opcodes */
		40, 41, 42, 43, 44,
		
		//
		// 5. Fragmentation:
		//
		
		45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
		
		//
		// 6. UTF-8 Handling:
		//
		
		/* - 6.1 Valid UTF-8 with zero payload fragments: */
		//65, 66, 67,
		/* - 6.2 Valid UTF-8 unfragmented, fragmented on code-points and within code-points */
		//68, 69, 70, 71,
		/* - 6.3 Invalid UTF-8 differently fragmented */
		//72, 73,
		/* - 6.4 Fail-fast on invalid UTF-8 */
		//74, 75, 76, 77,
		/* - 6.5 Some valid UTF-8 sequences */
		//78, 79, 80, 81, 82,
		/* - 6.6 All prefixes of a valid UTF-8 string that contains multi-byte code points */
		//83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,
		/* - 6.7 First possible sequence of a certain length */
		//94, 95, 96, 97,
		/* - 6.8 First possible sequence length 5/6 (invalid codepoints) */
		//98, 99, 100,
		/* - 6.9 Last possible sequence of a certain length */
		//101, 102, 103, 104,
		/* - 6.10 Last possible sequence length 4/5/6 (invalid codepoints) */
		//105, 106, 107,
		/* - 6.11 Other boundary conditions */
		//108, 109, 110, 111,
		/* - 6.12 Unexpected continuation bytes */
		//112, 113, 114, 115, 116, 117, 118, 119,
		/* - 6.13 Lonely start characters */
		//120, 121, 122, 123, 124,
		/* - 6.14 Sequences with last continuation byte missing */
		//125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 
		/* - 6.15 Concatenation of incomplete sequences */
		//135, 
		/* - 6.16 Impossible bytes */
		//136, 137, 138,
		/* - 6.17 Examples of an overlong ASCII character */
		//139, 140, 141, 142, 143,
		/* - 6.18 Maximum overlong sequences */
		//144, 145, 146, 147, 148,
		/* - 6.19 Overlong representation of the NUL character */
		//149, 150, 151, 152, 153,
		/* - 6.20 Single UTF-16 surrogates */
		//154, 155, 156, 157, 158, 159, 160,
		/* - 6.21 Paired UTF-16 surrogates */
		//161, 162, 163, 164, 165, 166, 167, 168,
		/* - 6.22 Non-character code points (valid UTF-8) */
		//169,170, 171, 172, 173, 174, 175, 176, 177, 178,
		//179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 
		//189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 
		//199, 200, 201, 202, 
		/* 6.23 Unicode specials (i.e. replacement char) */
		//203, 204, 205, 206, 207, 208, 209,
		
		//
		// 7. Close Handling:
		//
		
		/* 7.1 Basic close behavior (fuzzer initiated) */
		210, 211, 212, 213, 214, 215,
		/* 7.3 Close frame structure: payload length (fuzzer initiated) */
		216, 217, 218, 219, 220, 221,
		/* 7.5 Close frame structure: payload value (fuzzer initiated) */
		222,
		/* 7.7 Close frame structure: valid close codes (fuzzer initiated) */
		223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,
		/* 7.9 Close frame structure: invalid close codes (fuzzer initiated) */
		236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
		/* 7.13 Informational close information (fuzzer initiated) */
		247, 248,
		
		//
		// 9. Limits/Performance:
		//
		
		/* 9.7 Text Message Roundtrip Time (fixed number, increasing size) */
		//291, 292, 293, 294,
		/* 9.8 Binary Message Roundtrip Time (fixed number, increasing size) */
		//297, 298, 299, 300
	]
};

var id = 0;

function stopScript() {
	scriptThread.appendTextToConsole("Finished");
}

function timerSlot() {
	if (id < options.cases.length)
		performTest(options.cases[id++]);
	else {
		updateReport();
		scriptThread.stopScript();
	}
}

function updateReport() {
	scriptThread.sendString(
		"<" + options.server.ip + "," +
			options.server.port + "," +
			"/updateReports?agent=" +
			options.agent + ">"
	);
}

function performTest(id) {
	scriptThread.sendString(
		"<" + options.server.ip + "," + 
			options.server.port + "," + 
			"/runCase?case=" + id +
			"&agent=" + options.agent + ">");
}

var timer = scriptThread.createTimer();
timer.timeoutSignal.connect(eval("timerSlot"));
timer.start(1500);

scriptThread.appendTextToConsole('Script has started');