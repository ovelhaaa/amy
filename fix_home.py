with open('smk-s3/components/ui/screens/home_screen.cpp', 'r') as f:
    content = f.read()
content = content.replace('scope_', 'scope')
content = content.replace('} else if (dw <= 160) {h"', '} else if (dw <= 160) {')
with open('smk-s3/components/ui/screens/home_screen.cpp', 'w') as f:
    f.write(content)
