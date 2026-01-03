import os

def bundle():
    # In a real scenario, this would read CSS/JS files and inject them into HTML
    # For now, everything is already in index.html for simplicity as per the <100KB requirement
    # We just ensure the file exists and maybe gzip it if needed

    with open('index.html', 'r') as f:
        content = f.read()

    print(f"Bundled size: {len(content)} bytes")

    # Example: write to a build artifact
    with open('index.html.h', 'w') as f:
        # Create C header file content
        hex_content = ', '.join([f'0x{b:02x}' for b in content.encode('utf-8')])
        f.write(f'const unsigned char index_html[] = {{ {hex_content} }};\n')
        f.write(f'const unsigned int index_html_len = {len(content)};\n')

if __name__ == '__main__':
    bundle()
