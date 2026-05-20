// Compile a WebUI test program and save the binary for device upload
import { Lexer } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { CodeGenerator } from '../src/codegen.js';
import { writeFileSync } from 'fs';

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
    strcpy(devname, "TestDev");
    return 0;
}
`;

try {
    const tokens = new Lexer(source).tokenize();
    const ast = new Parser(tokens).parse();
    const result = new CodeGenerator().compile(ast);

    writeFileSync('/Volumes/vp_dev/TinyC/tests/webui_test.tcb', Buffer.from(result.binary));
    console.log(`Compiled: ${result.binary.length} bytes -> webui_test.tcb`);
    console.log('Functions:', Object.keys(result.functions));
    console.log('Constants:', result.constants);
} catch (e) {
    console.error('ERROR:', e.message);
    process.exit(1);
}
