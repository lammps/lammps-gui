## -*- Python -*- ######################################################################
## LAMMPS-GUI - A Graphical Tool to Learn and Explore the LAMMPS MD Simulation Software
##
## Copyright (c) 2023, 2024, 2025, 2026  Axel Kohlmeyer
##
## Documentation: https://lammps-gui.lammps.org/
## Contact: akohlmey@gmail.com
##
## This software is distributed under the GNU General Public License version 2 or later.
########################################################################################

import os
import pyautogui
import subprocess
import time
import unittest
from PIL import Image

pyautogui.PAUSE = 0.25

def tuple_compare(one,two,delta):
    """Compare two RGB triples to determine if they differ less than delta"""
    if abs(one[0]-two[0]) < delta and \
       abs(one[1]-two[1]) < delta and \
       abs(one[2]-two[2]) < delta:
        return True
    else:
        return False

def screenshot(fname):
    """Create a screenshot and save to the given file name fname"""
    return subprocess.check_output(["shooter", fname], stderr=subprocess.STDOUT)

def check_image(fname, color, delta):
    """
    Check if a square of four pixels of an image have the expected color
    with a given delta.  Delete the image if the pixels match.
    """
    with Image.open(fname) as im:
        im = im.convert("RGB")
        px = im.load()
        # check format, size, and that it is all one color
        if tuple_compare(px[60,75], color, delta) and \
           tuple_compare(px[930,75], color, delta) and \
           tuple_compare(px[930,400], color, delta) and \
           tuple_compare(px[60,400], color, delta):
            os.remove(fname)
            return True
        else:
            print(px[60,75], px[930,75], px[930,400], px[60,400], color)
            return False

def focus():
    """Give focus to the editor window by positioning the mouse and clicking"""
    pyautogui.click(button='left', x=100, y=100)

class GUIEditorChecks(unittest.TestCase):
    """This set of tests exercises some basic editor tasks"""

    # handle for LAMMPS-GUI process
    gui=None

    def setUp(self):
        """Launch LAMMPS-GUI and give it focus"""
        helptxt = subprocess.check_output([os.environ['LAMMPS_GUI'], '--platform', 'offscreen', '-h'],
                                          shell=False, stderr=subprocess.STDOUT).decode()
        # get exact path of the LAMMPS-GUI binary from environment variable
        cmdline = [os.environ['LAMMPS_GUI'], '-x', '1000', '-y', '500']
        # append path to LAMMPS shared library, if present
        if 'pluginpath' in helptxt:
            cmdline.append('-p')
            cmdline.append('liblammps.so.0')

        # launch LAMMPS-GUI and give it a moment to come up
        self.gui=subprocess.Popen(cmdline, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        time.sleep(1.0)
        for f in ['hello.txt', 'hello1.txt', 'hello2.txt', 'empty.txt', 'complete.txt',
                  'shot1.png', 'shot2.png']:
            if os.path.exists(f):
                os.remove(f)
        focus()

    def tearDown(self):
        """Stop LAMMPS-GUI"""
        if self.gui.poll() is None:
            self.gui.terminate()

    def testExitShortcut(self):
        """Exit LAMMPS-GUI immediately via keyboard shortcut"""
        screenshot("shot1.png")
        pyautogui.hotkey('ctrl','q')
        screenshot("shot2.png")
        self.assertEqual(self.gui.poll(), 0)
        self.assertTrue(check_image('shot1.png', (255,255,255), 10))
        self.assertTrue(check_image('shot2.png', (0,0,0), 1))

    def testExitMenu(self):
        """Exit LAMMPS-GUI immediately via the menu"""
        screenshot("shot1.png")
        pyautogui.hotkey('alt','f')
        pyautogui.press('q')
        screenshot("shot2.png")
        self.assertEqual(self.gui.poll(), 0)
        self.assertTrue(check_image('shot1.png', (255,255,255), 10))
        self.assertTrue(check_image('shot2.png', (0,0,0), 1))

    def testExitModCancelNo(self):
        """Exit LAMMPS-GUI with a modified buffer without saving"""
        pyautogui.typewrite("Hello, World!")
        pyautogui.press('enter')
        # cancel on first attempt to exit and we'll drop back to the editor
        pyautogui.hotkey('ctrl','q')
        pyautogui.hotkey('alt','c')
        # try to exit again and this time say "no"
        focus()
        pyautogui.hotkey('ctrl','q')
        pyautogui.hotkey('alt','n')
        self.assertEqual(self.gui.poll(), 0)

    @unittest.skip("unreliable interaction with the modal save dialog; needs rework")
    def testExitModSave(self):
        """Exit LAMMPS-GUI with a modified buffer and save it to a file"""
        # First enter some text
        pyautogui.typewrite("Hello, World!")
        pyautogui.press('enter')
        # try to exit and select "yes" to save
        pyautogui.hotkey('ctrl','q')
        pyautogui.hotkey('alt','y')
        # enter file name and save
        pyautogui.typewrite('hello.txt')
        time.sleep(0.2)
        pyautogui.press('enter')
        pyautogui.press('enter')
        time.sleep(0.2)
        self.assertTrue(os.path.exists('hello.txt'))
        with open('hello.txt','r') as f:
            lines = f.read().splitlines()
            self.assertEqual(lines[1],"Hello, World!")
            f.close()
            os.remove('hello.txt')
        self.assertEqual(self.gui.poll(), 0)

    @unittest.skip("unreliable interaction with the modal file dialogs; needs rework")
    def testEditSaveLoad(self):
        """Exercise various Load/Save/Save As/New file options"""
        pyautogui.hotkey('ctrl','a')
        pyautogui.press('delete')
        pyautogui.typewrite("Hello, World!\n")
        pyautogui.typewrite("Hello, LAMMPS!\n")
        # save edit to file
        pyautogui.hotkey('ctrl','s')
        pyautogui.typewrite('hello.txt')
        time.sleep(0.2)
        pyautogui.hotkey('enter')
        time.sleep(0.2)
        pyautogui.hotkey('enter')
        self.assertTrue(os.path.exists('hello.txt'))
        with open('hello.txt','r') as f:
            lines = f.read().splitlines()
            self.assertEqual(lines[0],"Hello, World!")
            self.assertEqual(lines[1],"Hello, LAMMPS!")
            f.close()

        # start new file and remove comment
        focus()
        pyautogui.hotkey('ctrl','n')
        pyautogui.hotkey('ctrl','a')
        pyautogui.press('delete')
        # save empty buffer to file
        pyautogui.hotkey('ctrl','s')
        pyautogui.typewrite('empty.txt')
        time.sleep(0.2)
        pyautogui.hotkey('enter')
        time.sleep(0.2)
        pyautogui.hotkey('enter')
        self.assertTrue(os.path.exists('empty.txt'))
        with open('empty.txt','r') as f:
            lines = f.read().splitlines()
            self.assertEqual(lines[0],"")
            f.close()
            os.remove('empty.txt')
        focus()
        # abandon buffer and open existing file
        pyautogui.hotkey('ctrl','o')
        pyautogui.typewrite('hello.txt')
        pyautogui.press('enter')
        focus()
        pyautogui.press('down')
        pyautogui.press('down')
        pyautogui.press('down')
        pyautogui.typewrite("Hello, LAMMPS-GUI!")
        pyautogui.press('enter')
        focus()
        # update current file on disk
        pyautogui.hotkey('ctrl','s')
        focus()
        time.sleep(0.2)
        # write same content to new name
        pyautogui.hotkey('ctrl','shift','s')
        time.sleep(0.2)
        pyautogui.typewrite('hello1.txt')
        pyautogui.press('enter')
        pyautogui.press('enter')
        time.sleep(0.2)

        # check files and content
        self.assertTrue(os.path.exists('hello.txt'))
        self.assertTrue(os.path.exists('hello1.txt'))

        with open('hello.txt','r') as f:
            lines = f.read().splitlines()
            self.assertEqual(lines[0],"Hello, World!")
            self.assertEqual(lines[1],"Hello, LAMMPS!")
            self.assertEqual(lines[2],"Hello, LAMMPS-GUI!")
            f.close()
            os.remove('hello.txt')

        with open('hello1.txt','r') as f:
            lines = f.read().splitlines()
            self.assertEqual(lines[0],"Hello, World!")
            self.assertEqual(lines[1],"Hello, LAMMPS!")
            self.assertEqual(lines[2],"Hello, LAMMPS-GUI!")
            f.close()
            os.remove('hello1.txt')

        # close LAMMPS-GUI
        focus()
        pyautogui.hotkey('ctrl','q')
        time.sleep(0.2)
        self.assertEqual(self.gui.poll(), 0)

    @unittest.skip("depends on completion popup timing; needs rework")
    def testEditCompletion(self):
        """Exercise the editor auto-completion popup"""
        # clear buffer
        pyautogui.hotkey('ctrl','a')
        pyautogui.press('delete')
        pyautogui.typewrite('ato')
        pyautogui.press('down')
        pyautogui.press('down')
        pyautogui.press('enter')
        pyautogui.press('space')
        pyautogui.typewrite('mol')
        pyautogui.press('enter')
        pyautogui.press('tab')
        pyautogui.press('enter')
        pyautogui.typewrite('uni')
        pyautogui.press('enter')
        pyautogui.press('space')
        pyautogui.typewrite('met')
        pyautogui.press('enter')
        pyautogui.press('tab')
        pyautogui.press('enter')
        pyautogui.typewrite('reg')
        pyautogui.press('enter')
        pyautogui.press('space')
        pyautogui.typewrite('box bl')
        pyautogui.press('enter')
        pyautogui.typewrite(' 0 1 0 1 0 1')
        pyautogui.press('tab')
        pyautogui.press('enter')

        # save edit to file
        pyautogui.hotkey('ctrl','s')
        pyautogui.typewrite('complete.txt')
        time.sleep(0.2)
        pyautogui.hotkey('enter')
        time.sleep(0.2)
        pyautogui.hotkey('enter')
        self.assertTrue(os.path.exists('complete.txt'))
        with open('complete.txt','r') as f:
            lines = f.read().splitlines()
            self.assertEqual(lines[0],"atom_style      molecular")
            self.assertEqual(lines[1],"units           metal")
            self.assertEqual(lines[2],"region          box block 0 1 0 1 0 1")
            f.close()
            os.remove('complete.txt')
        # close LAMMPS-GUI
        focus()
        pyautogui.hotkey('ctrl','q')
        time.sleep(0.2)
        self.assertEqual(self.gui.poll(), 0)

##############################
if __name__ == "__main__":
    unittest.main(verbosity=2)
