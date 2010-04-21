"""Tests the reference building of the cactus pipeline.
"""

import unittest
import os
import sys

from sonLib.bioio import parseSuiteTestOptions
from sonLib.bioio import TestStatus
from sonLib.bioio import getTempDirectory
from sonLib.bioio import logger
from sonLib.bioio import system

from cactus.shared.test import getCactusInputs_random
from cactus.shared.test import getCactusInputs_blanchette
from cactus.shared.test import runWorkflow_TestScript

class TestCase(unittest.TestCase):
    def setUp(self):
        self.testNo = TestStatus.getTestSetup(1, 5, 20, 100)
        unittest.TestCase.setUp(self)
    
    def testCactusPhylogeny_Random(self):
        """Runs the tests across some simulated regions.
        """
        tempDir = getTempDirectory(os.getcwd())
        for test in xrange(self.testNo): 
            sequences, newickTreeString = getCactusInputs_random(tempDir)
            runWorkflow_TestScript(sequences, newickTreeString, tempDir, 
                                   buildTrees=False, buildReference=True, 
                                   buildFaces=False)
            logger.info("Finished random test %i" % test)
        system("rm -rf %s" % tempDir)
    
    def testCactusWorkflow_Blanchette(self): 
        """Runs the workflow on blanchette's simulated (colinear) regions.
        """
        tempDir = getTempDirectory(os.getcwd())
        sequences, newickTreeString = getCactusInputs_blanchette()
        runWorkflow_TestScript(sequences, newickTreeString, tempDir,
                               buildTrees=False, buildReference=True, 
                               buildFaces=False)
        system("rm -rf %s" % tempDir)
        logger.info("Finished blanchette test")
        
def main():
    parseSuiteTestOptions()
    sys.argv = sys.argv[:1]
    unittest.main()
        
if __name__ == '__main__':
    main()
