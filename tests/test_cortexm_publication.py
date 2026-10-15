"""Host publication admission only; fixture compiler never proves guest lowering."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class CortexMOutputAdmission(unittest.TestCase):
    def test_existing_and_dangling_output_symlinks_are_preserved(self):
        script = Path(__file__).resolve().parents[1]/'scripts/compile-cortexm-object.py'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker = root/'compiler-invoked'
            compiler = root/'compiler'
            compiler.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nPath('+repr(str(marker))+').touch()\n')
            compiler.chmod(0o755)
            source = root/'source.ores'
            source.write_text('source fixture')
            target = root/'valuable.o'
            target.write_bytes(b'preserve existing object')
            for destination in (target, root/'absent.o'):
                output = root/'output.o'
                output.symlink_to(destination)
                result = subprocess.run([sys.executable,str(script),'--compiler',str(compiler),
                    '--clang',str(compiler),'--source',str(source),'--output',str(output)],
                    capture_output=True,text=True,timeout=5)
                self.assertNotEqual(result.returncode,0)
                self.assertIn('output destination must not be a symlink',result.stderr)
                self.assertTrue(output.is_symlink())
                self.assertFalse(marker.exists())
                self.assertEqual(target.read_bytes(),b'preserve existing object')
                self.assertFalse((root/'absent.o').exists())
                output.unlink()

    def test_live_compiler_output_budget_preserves_existing_object(self):
        script = Path(__file__).resolve().parents[1]/'scripts/compile-cortexm-object.py'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root/'source.ores'
            source.write_text('source fixture')
            output = root/'output.o'
            output.write_bytes(b'previous object')
            for fd in (1,2):
                compiler = root/'compiler'
                compiler.write_text('#!'+sys.executable+'\nimport os,time\nos.write('+str(fd)+", b'x' * (3*1024*1024))\ntime.sleep(20)\n")
                compiler.chmod(0o755)
                result = subprocess.run([sys.executable,str(script),'--compiler',str(compiler),
                    '--clang',str(compiler),'--source',str(source),'--output',str(output)],
                    capture_output=True,text=True,timeout=5)
                self.assertNotEqual(result.returncode,0)
                self.assertIn('compiler output budget exceeded',result.stderr)
                self.assertEqual(output.read_bytes(),b'previous object')
                self.assertEqual(list(root.glob('.ores-arm-*')),[])


if __name__=='__main__':
    unittest.main()
