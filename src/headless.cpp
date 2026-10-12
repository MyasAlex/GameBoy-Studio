#include "gameboy.hpp"
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: gb_headless ROM.gb [frames] [--test] [--output frame.ppm] [--grey]\n";
        return 0;
    }
    int frames = 120;
    bool test = false, mooneye = false;
    std::filesystem::path output;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--test") test = true;
        else if (arg == "--mooneye") mooneye = true;
        else if (arg == "--output" && i + 1 < argc) output = argv[++i];
        else if (arg != "--grey") {
            try { frames = std::clamp(std::stoi(arg), 1, 100000); }
            catch (...) { std::cerr << "Invalid argument: " << arg << '\n'; return 2; }
        }
    }
    GameBoy gb;
    if (!gb.load(argv[1])) { std::cerr << "Could not load ROM: " << gb.mem.loadError << '\n'; return 2; }
    // Some hardware test cartridges intentionally omit a RAM-size declaration.
    // Provide their test-board RAM only in test mode, never for ordinary games.
    if (test && gb.mem.eram.empty() && gb.mem.mbc == 1) gb.mem.eram.assign(8192,0xFF);
    for (int i = 2; i < argc; ++i) if (std::string(argv[i]) == "--grey") gb.setPalette(1);
    bool passed = false, failed = false;
    int completed = 0;
    std::size_t printed = 0;
    for (int i = 0; i < frames; ++i) {
        if (mooneye) {
            int t = 0;
            while (t < GB_FRAME) {
                if (gb.mem.read(gb.cpu.PC) == 0x40) {
                    const auto& c = gb.cpu;
                    passed = c.B==3 && c.C==5 && c.D==8 && c.E==13 && c.H==21 && c.L==34;
                    failed = c.B==0x42 && c.C==0x42 && c.D==0x42 && c.E==0x42 && c.H==0x42 && c.L==0x42;
                    if (passed || failed) break;
                }
                t += gb.cpu.step();
            }
        } else gb.runFrame();
        completed = i + 1;
        if (gb.mem.serialOutput.size() > printed) {
            std::cout << gb.mem.serialOutput.substr(printed) << std::flush;
            printed = gb.mem.serialOutput.size();
        }
        if (mooneye && (passed || failed)) break;
        if (test) {
            const auto& serial = gb.mem.serialOutput;
            passed = serial.find("Passed") != std::string::npos;
            failed = serial.find("Failed") != std::string::npos || serial.find("FAILED") != std::string::npos;
            if (gb.mem.eram.size() >= 4 && gb.mem.eram[1] == 0xDE &&
                gb.mem.eram[2] == 0xB0 && gb.mem.eram[3] == 0x61 && gb.mem.eram[0] < 0x80) {
                passed = gb.mem.eram[0] == 0;
                failed = !passed;
                for (std::size_t n = 4; n < gb.mem.eram.size() && gb.mem.eram[n]; ++n)
                    std::cout << (char)gb.mem.eram[n];
                std::cout << '\n';
            }
            if (passed || failed) break;
        }
    }
    std::cout << "\nframes=" << completed << " PC=" << std::hex << gb.cpu.PC << std::dec
              << " LY=" << (int)gb.mem.io[0x44] << " ROM=" << gb.mem.gameTitle << '\n';
    if (!output.empty()) {
        std::ofstream f(output, std::ios::binary);
        if (!f) { std::cerr << "Could not write screenshot\n"; return 2; }
        f << "P6\n160 144\n255\n";
        for (u32 px : gb.ppu.pixels) {
            const char rgb[3] = {(char)(px >> 16), (char)(px >> 8), (char)px}; f.write(rgb, 3);
        }
        if (!f) return 2;
    }
    if (test || mooneye) {
        std::cout << (passed ? "RESULT: PASS\n" : failed ? "RESULT: FAIL\n" : "RESULT: TIMEOUT\n");
        return passed ? 0 : 1;
    }
    return 0;
}
