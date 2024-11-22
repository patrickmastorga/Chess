import numpy as np
import ctypes
import torch

from feature_sets import *

dll = ctypes.cdll.LoadLibrary("C:\\Users\\patri\\Documents\\GitHub\\chess2024\\build\\Release\\data_loader.dll")

# TrainingDataStream *create_stream(const char *path, float drop, size_t worker_id, size_t num_workers)
create_stream = dll.create_stream
create_stream.argtypes = [ctypes.c_char_p, ctypes.c_float, ctypes.c_size_t, ctypes.c_size_t]
create_stream.restype = ctypes.c_void_p

# void destroy_stream(TrainingDataStream *stream)
destroy_stream = dll.destroy_stream
destroy_stream.argtypes = [ctypes.c_void_p]

# Basic Batch

# BasicFeatureSetBatch *get_basic_batch(TrainingDataStream *stream, size_t size)
get_basic_batch = dll.get_basic_batch
get_basic_batch.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
get_basic_batch.restype = BasicTrainingDataBatchPtr

# void destroy_basic_batch(BasicFeatureSetBatch *batch)
destroy_basic_batch = dll.destroy_basic_batch
destroy_basic_batch.argtypes = [BasicTrainingDataBatchPtr]

class TrainingDataBatchDataset(torch.utils.data.IterableDataset):
    def __init__(self, path: str, batch_size, drop=0, feature_set="basic"):
        super().__init__()
        self.path = ctypes.c_char_p(path.encode('utf-8'))
        self.batch_size = ctypes.c_size_t(batch_size)
        self.drop = ctypes.c_float(drop)
        self.stream = None

        match feature_set:
            case "basic":
                self.get_batch = get_basic_batch
                self.destroy_batch = destroy_basic_batch
            case _:
                raise ValueError("Unsupported or unknown feature set!")
    
    def __iter__(self):
        worker_info = torch.utils.data.get_worker_info()
        if worker_info is None:
            worker_id = ctypes.c_size_t(0)
            num_workers = ctypes.c_size_t(0)
        else:
            worker_id = ctypes.c_size_t(worker_info.id)
            num_workers = ctypes.c_size_t(worker_info.num_workers)

        self.stream = create_stream(self.path, self.drop, worker_id, num_workers)
        return self
    
    def __next__(self):
        batch = self.get_batch(self.stream, self.batch_size)
        if not batch:
            destroy_stream(self.stream)
            self.stream = None
            raise StopIteration

        tensors = batch.contents.get_tensors()
        self.destroy_batch(batch)
        return tensors
    
    def __del__(self):
        if self.stream is not None:
            destroy_stream(self.stream)
            self.stream = None