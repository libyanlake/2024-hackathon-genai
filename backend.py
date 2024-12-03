from flask import Flask, request, jsonify
import tensorflow as tf
from tensorflow.keras.applications import EfficientNetB0
from tensorflow.keras.applications.efficientnet import preprocess_input, decode_predictions
from tensorflow.keras.preprocessing.image import load_img, img_to_array
import numpy as np
import os

# Initialize Flask app
app = Flask(__name__)

# Load the EfficientNetB0 model pre-trained on ImageNet
model = EfficientNetB0(weights='imagenet')

# Function to preprocess the input image
def preprocess_image(image_path):
    # Load the image with target size 224x224 (EfficientNetB0 input size)
    img = load_img(image_path, target_size=(224, 224))
    # Convert the image to a numpy array
    img_array = img_to_array(img)
    # Add a batch dimension (EfficientNetB0 expects a batch of images)
    img_array = np.expand_dims(img_array, axis=0)
    # Preprocess the image (normalizes input as expected by EfficientNetB0)
    img_array = preprocess_input(img_array)
    return img_array

# Function to make predictions
def predict_image(image_path):
    # Preprocess the image
    preprocessed_image = preprocess_image(image_path)
    # Get predictions
    predictions = model.predict(preprocessed_image)
    # Decode the predictions to human-readable labels
    decoded_predictions = decode_predictions(predictions, top=3)[0]
    return decoded_predictions

# Endpoint to receive and classify images
@app.route('/predict', methods=['POST'])
def predict():
    if 'image' not in request.files:
        return jsonify({'error': 'No image file provided'}), 400
    
    file = request.files['image']
    image_path = os.path.join('/tmp', file.filename)  # Temporary storage for the uploaded image
    file.save(image_path)  # Save the uploaded image

    try:
        # Make prediction
        predictions = predict_image(image_path)
        # Format the predictions for JSON response
        results = [
            {'label': label, 'description': description, 'score': float(score)}
            for label, description, score in predictions
        ]
        return jsonify(results)
    except Exception as e:
        return jsonify({'error': str(e)}), 500
    finally:
        if os.path.exists(image_path):  # Clean up the temporary file
            os.remove(image_path)

# Run the Flask app
if __name__ == '__main__':
    app.run(debug=True)
