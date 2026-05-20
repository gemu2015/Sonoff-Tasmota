// Test: WebUI widget compilation and VM execution
import { Lexer } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { CodeGenerator } from '../src/codegen.js';
import { VM } from '../src/vm.js';

const source = `
int relay;
int brightness;
int mode;
int alarm_time;
char devname[32];

void WebUI() {
    wButton(relay, "Power");
    wSlider(brightness, 0, 100, "Brightness");
    wCheckbox(mode, "Auto Mode");
    wText(devname, 32, "Device Name");
    wNumber(brightness, 0, 100, "Set Level");
    wPulldown(mode, "Off|Auto|Manual|Timer");
    wRadio(mode, "Off|Auto|Manual");
    wTime(alarm_time, "Wake-up");
}

int main() {
    relay = 1;
    brightness = 75;
    mode = 2;
    alarm_time = 730;
    strcpy(devname, "Living Room");
    return 0;
}
`;

let output = '';
try {
    // Compile
    const tokens = new Lexer(source).tokenize();
    const ast = new Parser(tokens).parse();
    const result = new CodeGenerator().compile(ast);

    console.log('=== Compilation OK ===');
    console.log(`Binary: ${result.binary.length} bytes`);
    console.log(`Globals:`, result.globals);
    console.log(`Functions:`, result.functions);
    console.log(`Constants:`, result.constants);

    // Check WebUI is in the callback list
    const hasWebUI = result.functions['WebUI'];
    console.log(`WebUI callback: ${hasWebUI ? 'found at addr ' + hasWebUI.address : 'MISSING!'}`);

    // Run VM
    const vm = new VM();
    vm.onOutput = (text) => {
        output += text;
        process.stdout.write(text);
    };
    vm.load(result);
    vm.run(100000);

    console.log('\n=== Main() finished, invoking WebUI() directly ===');

    // Manually invoke WebUI callback by resetting PC to its address
    const webUIAddr = result.functions['WebUI'];
    if (webUIAddr) {
        vm.pc = result.codeOffset + webUIAddr.address;
        vm.halted = false;
        vm.sp = 0;  // clean stack
        vm.frameCount = 0;
        vm.run(100000);
    }

    console.log('\n=== Output ===');
    console.log(output);

    // Verify globals were set
    console.log('\n=== Global values after main() ===');
    const g = result.globals;
    console.log(`relay (idx=${g.relay.index}): ${vm.globals[g.relay.index]}`);
    console.log(`brightness (idx=${g.brightness.index}): ${vm.globals[g.brightness.index]}`);
    console.log(`mode (idx=${g.mode.index}): ${vm.globals[g.mode.index]}`);
    console.log(`alarm_time (idx=${g.alarm_time.index}): ${vm.globals[g.alarm_time.index]}`);

    // Read devname string from globals
    let name = '';
    const dnIdx = g.devname.index;
    for (let i = 0; i < 32; i++) {
        const ch = vm.globals[dnIdx + i];
        if (ch === 0) break;
        name += String.fromCharCode(ch);
    }
    console.log(`devname (idx=${dnIdx}): "${name}"`);

    console.log('\n=== ALL TESTS PASSED ===');
} catch (e) {
    console.error('ERROR:', e.message);
    console.error(e.stack);
    process.exit(1);
}
