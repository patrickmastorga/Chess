import numpy as np
import ctypes
import torch

class BasicTrainingDataBatch(ctypes.Structure):
    _fields_ = [
        ('size', ctypes.c_size_t),
        ('input', ctypes.POINTER(ctypes.c_float)),
        ('score', ctypes.POINTER(ctypes.c_float)),
    ]

    def get_tensors(self):
        input = torch.from_numpy(np.ctypeslib.as_array(self.input, shape=(self.size, 12, 8, 8))).pin_memory()
        score = torch.from_numpy(np.ctypeslib.as_array(self.score, shape=(self.size, 1))).pin_memory()
        return input, score

BasicTrainingDataBatchPtr = ctypes.POINTER(BasicTrainingDataBatch)