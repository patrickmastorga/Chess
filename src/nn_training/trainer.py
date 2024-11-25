import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader
import matplotlib.pyplot as plt
from conv_net.model import ChessEvaluationCNN
from training_data.dataset import TrainingDataBatchDataset

# Configurable constants
DATA_PATH = "C:\\Users\\patri\\Documents\\GitHub\\chess2024\\src\\nn_training\\training_data\\datasets\\test80-2024-02-feb.binpack"  # Path to the training data
BATCH_SIZE = 256                    # Batch size for training
EPOCHS = 5                          # Number of epochs
VERSION = "1.0"                     # Version number for saving the model
BASE_MODEL_PATH = None              # Path to a base model (set to None if not using)
LEARNING_RATE = 1e-4                # Learning rate for the optimizer
NUM_WORKERS = 4                     # Number of workers for the dataset

# Model saving directory
MODEL_SAVE_DIR = "conv_net/models"
os.makedirs(MODEL_SAVE_DIR, exist_ok=True)

# Initialize the model
model = ChessEvaluationCNN()

# Load parameters from a base model if specified
if BASE_MODEL_PATH:
    if os.path.exists(BASE_MODEL_PATH):
        model.load_state_dict(torch.load(BASE_MODEL_PATH))
        print(f"Loaded parameters from base model: {BASE_MODEL_PATH}")
    else:
        print(f"Base model not found at: {BASE_MODEL_PATH}")
        exit(1)

# Define dataset and DataLoader
dataset = TrainingDataBatchDataset(path=DATA_PATH, batch_size=BATCH_SIZE, num_workers=NUM_WORKERS)

# Define loss function and optimizer
criterion = nn.MSELoss()
optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)

# Training setup
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
model.to(device)

# Track loss for plotting
batch_losses = []
cumulative_loss = 0.0

# Training loop
for epoch in range(EPOCHS):
    model.train()
    print(f"Starting epoch {epoch + 1}/{EPOCHS}")
    
    for batch_idx, (inputs, targets) in enumerate(dataset):
        inputs, targets = inputs.to(device), targets.to(device)

        # Forward pass
        outputs = model(inputs)
        loss = criterion(outputs, targets)

        # Backward pass and optimization
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

        # Track loss
        cumulative_loss += loss.item()
        if (batch_idx + 1) % 200000 == 0:
            avg_loss = cumulative_loss / 200000
            batch_losses.append(avg_loss)
            print(f"Batch {batch_idx + 1}: Average Loss = {avg_loss:.6f}")
            cumulative_loss = 0.0

    # Save model after each epoch
    model_save_path = os.path.join(MODEL_SAVE_DIR, f"model_v{VERSION}_epoch{epoch + 1}.pth")
    torch.save(model.state_dict(), model_save_path)
    print(f"Model saved to {model_save_path}")

# Plot the training loss
plt.figure(figsize=(10, 6))
plt.plot(range(1, len(batch_losses) + 1), batch_losses, marker='o')
plt.title("Training Loss Over Time")
plt.xlabel("Checkpoint (every 200,000 batches)")
plt.ylabel("Average Loss")
plt.grid()
plt.savefig(os.path.join(MODEL_SAVE_DIR, f"loss_plot_v{VERSION}.png"))
plt.show()
