#!/usr/bin/env python3
"""
AMY Studio - Local HTTP Server with Cross-Origin Isolation
Enables SharedArrayBuffer and multithreaded WebAssembly / AudioWorklet execution.
Usage: python server.py [port]
"""

import sys
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler

class AMYStudioRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        # Enable Cross-Origin Isolation for AudioWorklet & SharedArrayBuffer
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', '*')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

def run_server(port=8080):
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    server_address = ('', port)
    httpd = HTTPServer(server_address, AMYStudioRequestHandler)
    print(f"============================================================")
    print(f"   AMY Studio Synthesizer Host & ESP32 Manager running at:")
    print(f"   http://localhost:{port}/")
    print(f"============================================================")
    print("Press Ctrl+C to stop the server.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server.")
        httpd.server_close()

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    run_server(port)
