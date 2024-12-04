# backend script:

from flask import Flask, request, jsonify
import tensorflow as tf
from tensorflow.keras.applications import EfficientNetB0
from tensorflow.keras.applications.efficientnet import preprocess_input, decode_predictions
from tensorflow.keras.preprocessing.image import load_img, img_to_array
import numpy as np
import tempfile
import os
from pyngrok import ngrok # if not using localhost

app = Flask(__name__)
model = EfficientNetB0(weights='imagenet')

# Function to preprocess the input image
def preprocess_image(image_path):
    img = load_img(image_path, target_size=(224, 224))
    # Convert the image to a numpy array
    img_array = img_to_array(img)
    # Add a batch dimension (EfficientNetB0 expects a batch of images)
    img_array = np.expand_dims(img_array, axis=0)
    img_array = preprocess_input(img_array)
    return img_array

def predict_image(image_path):
    preprocessed_image = preprocess_image(image_path)
    predictions = model.predict(preprocessed_image)
    decoded_predictions = decode_predictions(predictions, top=3)[0]
    return decoded_predictions[0]

@app.route('/predict', methods=['POST'])
def predict():
    if 'image' not in request.files:
        return 'No image file provided', 400

    file = request.files['image']

    if not file.filename.lower().endswith(('png', 'jpg', 'jpeg')):
        return 'Unsupported file type. Please upload a PNG, JPG, or JPEG image.', 400

    try:
        with tempfile.NamedTemporaryFile(delete=False, suffix='.jpg') as temp_file:
            file.save(temp_file.name)
            image_path = temp_file.name

        prediction = predict_image(image_path)
        return prediction[1]  # Return only the description (e.g., "African_elephant")

    except Exception as e:
        return str(e), 500

    finally:
        if os.path.exists(image_path):
            os.remove(image_path)



if __name__ == '__main__':
    public_url = ngrok.connect(5000) # if not using localhost
    print(f"Public URL: {public_url}") # ^
    app.run()
