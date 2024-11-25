import numpy as np
import ctypes
import torch

dll = ctypes.cdll.LoadLibrary("/home/hice1/dkeskinyan3/astorga/chess2024/build/libdata_loader.so")

# Basic Batch
class BasicTrainingDataBatch(ctypes.Structure):
    _fields_ = [
        ('size', ctypes.c_size_t),
        ('input', ctypes.POINTER(ctypes.c_float)),
        ('score', ctypes.POINTER(ctypes.c_float)),
    ]

    def get_tensors(self):
        input = torch.from_numpy(np.ctypeslib.as_array(self.input, shape=(self.size, 12, 8, 8))).pin_memory()
        score = torch.from_numpy(np.ctypeslib.as_array(self.score, shape=(self.size, 1))).pin_memory()
        return input, torch.tanh(score / 800)
    
# DataLoader<BasicFeatureSetBatch> *create_basic_data_loader(const char *path, size_t batch_size, float drop, size_t num_workers) noexcept
create_basic_data_loader = dll.create_basic_data_loader
create_basic_data_loader.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_float, ctypes.c_size_t]
create_basic_data_loader.restype = ctypes.c_void_p

# void destroy_basic_data_loader(DataLoader<BasicFeatureSetBatch> *data_loader) noexcept
destroy_basic_data_loader = dll.destroy_basic_data_loader
destroy_basic_data_loader.argtypes = [ctypes.c_void_p]

# BasicFeatureSetBatch *get_basic_batch(DataLoader<BasicFeatureSetBatch> *data_loader) noexcept
get_basic_batch = dll.get_basic_batch
get_basic_batch.argtypes = [ctypes.c_void_p]
get_basic_batch.restype = ctypes.POINTER(BasicTrainingDataBatch)

# void destroy_basic_batch(BasicFeatureSetBatch *batch) noexcept
destroy_basic_batch = dll.destroy_basic_batch
destroy_basic_batch.argtypes = [ctypes.POINTER(BasicTrainingDataBatch)]

class TrainingDataBatchDataset(torch.utils.data.IterableDataset):
    def __init__(self, path: str, batch_size, drop=0, num_workers=1, feature_set="basic"):
        super().__init__()
        self.path = ctypes.c_char_p(path.encode('utf-8'))
        self.batch_size = ctypes.c_size_t(batch_size)
        self.drop = ctypes.c_float(drop)
        self.num_workers = ctypes.c_size_t(num_workers)
        self.data_loader = None

        if feature_set != "basic":
            raise ValueError("Unsupported or unknown feature set!")

        self.create_data_loader = create_basic_data_loader
        self.destroy_data_loader = destroy_basic_data_loader
        self.get_batch = get_basic_batch
        self.destroy_batch = destroy_basic_batch
    
    def __iter__(self):
        self.data_loader = create_basic_data_loader(self.path, self.batch_size, self.drop, self.num_workers)
        return self
    
    def __next__(self):
        batch = self.get_batch(self.data_loader)
        if not batch:
            self.destroy_data_loader(self.data_loader)
            self.data_loader = None
            raise StopIteration

        tensors = batch.contents.get_tensors()
        self.destroy_batch(batch)
        return tensors
    
    def __del__(self):
        if self.data_loader is not None:
            self.destroy_data_loader(self.data_loader)
            self.data_loader = None