"""Reuse CI binaries only when compiled inputs and native build commands match."""
import os
from pathlib import Path
import re
import subprocess


def git(*args):
    return subprocess.check_output(['git', *args], text=True)


def native_recipe(text):
    steps = re.split(r'^      - ', text, flags=re.M)
    recipes = []
    for name in ('Build universal static TLS dependency', 'Configure universal VST3',
                 'Build plugin and test programs'):
        block = next(s for s in steps if s.startswith('name: ' + name + '\n'))
        recipes.append(block[block.index('        run:'):].strip())
    recipes += [s.strip() for s in steps if 'repository:' in s]
    recipes.append(re.search(r'^    runs-on: .+$', text, re.M).group())
    return recipes


source = os.environ['GITHUB_SHA']
reuse = False
try:
    match = re.fullmatch(r'lsl-mac-universal-v1-([0-9a-f]{40})', os.environ.get('NATIVE_CACHE_KEY', ''))
    if match:
        previous = match[1]
        unchanged = subprocess.run(['git', 'diff', '--quiet', previous, 'HEAD', '--',
                                    'Source', 'CMakeLists.txt', 'Tests/*.cpp']).returncode == 0
        workflow = '.github/workflows/build-mac-vst3.yml'
        same_recipe = native_recipe(git('show', previous + ':' + workflow)) == native_recipe(Path(workflow).read_text())
        present = all(Path(p).exists() for p in (
            'build/Release/lsl_media_test',
            'build/lsl_plugin_test_artefacts/Release/lsl_plugin_test',
            'build/LindellStreamsLive_artefacts/Release/VST3/Lindell Streams Live.vst3',
            'build/Release/lsl_core_tests', 'build/Release/lsl_link_tests'))
        reuse = unchanged and same_recipe and present
        if reuse:
            source = previous
except (OSError, subprocess.SubprocessError, ValueError, StopIteration, AttributeError):
    pass  # A missing or incompatible cache always requires a complete build.
with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
    output.write(f'reuse={str(reuse).lower()}\nsource={source}\n')
print('Reuse verified native inputs' if reuse else 'Complete native build required')
