#!/usr/bin/env python3

import tensorflow as tf
import sys
sys.path.append("../tensorflow/tensorflow/examples/speech_commands/")
import input_data
import models
import numpy as np
import pickle

from config import *
if len(sys.argv) != 2:
    print("Incorrect number of arguments")
    sys.exit()
cmd = sys.argv[1]

# Helper function to run inference
def run_tflite_inference_testSet(tflite_model_path, model_type="Float"):
    #
    # Load test data
    #
    np.random.seed(0) # set random seed for reproducible test results.
    with tf.compat.v1.Session() as sess:
        test_data, test_labels = audio_processor.get_data(
            -1, 0, model_settings, BACKGROUND_FREQUENCY, BACKGROUND_VOLUME_RANGE,
            TIME_SHIFT_MS, 'testing', sess
        )
    test_data = np.expand_dims(test_data, axis=1).astype(np.float32)

    #
    # Initialize the interpreter
    #
    interpreter = tf.lite.Interpreter(tflite_model_path)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]

    #
    # For quantized models, manually qantize the input data from float to integer
    #
    if model_type == "Quantized":
        input_scale, input_zero_point = input_details["quantization"]
        test_data = test_data / input_scale + input_zero_point
        test_data = test_data.astype(input_details["dtype"])

    #
    # Evaluate the predictions
    #
    correct_predictions = 0
    for i in range(len(test_data)):
        interpreter.set_tensor(input_details["index"], test_data[i])
        interpreter.invoke()
        output = interpreter.get_tensor(output_details["index"])[0]
        top_prediction = output.argmax()
        correct_predictions += (top_prediction == test_labels[i])

    print(f"{model_type} model accuracy is {(correct_predictions * 100) / len(test_data)}% (Number of test samples={len(test_data)})")
import subprocess
if cmd == "train":
  subprocess.run(["python3", "../tensorflow/tensorflow/examples/speech_commands/train.py",
               f"--data_dir={DATASET_DIR}",
               f"--wanted_words={WANTED_WORDS}",
               f"--silence_percentage={SILENT_PERCENTAGE}",
               f"--unknown_percentage={UNKNOWN_PERCENTAGE}",
               f"--preprocess={PREPROCESS}",
               f"--window_stride={WINDOW_STRIDE}",
               f"--model_architecture={MODEL_ARCHITECTURE}",
               f"--how_many_training_steps={TRAINING_STEPS}",
               f"--learning_rate={LEARNING_RATE}",
               f"--train_dir={TRAIN_DIR}",
               f"--summaries_dir={LOGS_DIR}",
               f"--verbosity={VERBOSITY}",
               f"--eval_step_interval={EVAL_STEP_INTERVAL}",
               f"--save_step_interval={SAVE_STEP_INTERVAL}"
               ])

if cmd == "freeze":
  subprocess.run(["python3", "../tensorflow/tensorflow/examples/speech_commands/freeze.py",
               f"--wanted_words={WANTED_WORDS}",
               f"--window_stride={WINDOW_STRIDE}",
               f"--preprocess={PREPROCESS}",
               f"--model_architecture={MODEL_ARCHITECTURE}",
               f"--start_checkpoint={TRAIN_DIR}{MODEL_ARCHITECTURE}.ckpt-{TOTAL_STEPS}",
               f"--save_format=saved_model",
               f"--output_file={SAVED_MODEL}"
                ])

if cmd == "convert":
    subprocess.run(["xxd", "-i", MODEL_TFLITE, MODEL_TFLITE_MICRO])
    REPLACE_TEXT = MODEL_TFLITE.replace('/', '_').replace('.', '_')
    subprocess.run(["sed", "-i", f"s/{REPLACE_TEXT}/g_model/g", MODEL_TFLITE_MICRO])

if cmd == "eval":
    model_settings = models.prepare_model_settings(
        len(input_data.prepare_words_list(WANTED_WORDS.split(','))),
        SAMPLE_RATE, CLIP_DURATION_MS, WINDOW_SIZE_MS,
        WINDOW_STRIDE, FEATURE_BIN_COUNT, PREPROCESS
    )
    audio_processor = input_data.AudioProcessor(
        DATA_URL, DATASET_DIR,
        SILENT_PERCENTAGE, UNKNOWN_PERCENTAGE,
        WANTED_WORDS.split(','), VALIDATION_PERCENTAGE,
        TESTING_PERCENTAGE, model_settings, LOGS_DIR
    )

    with tf.compat.v1.Session() as sess:
        float_converter = tf.lite.TFLiteConverter.from_saved_model(SAVED_MODEL)
        float_tflite_model = float_converter.convert()
        float_tflite_model_size = open(FLOAT_MODEL_TFLITE, "wb").write(float_tflite_model)
        print(f"Float model is {float_tflite_model_size} bytes")

        converter = tf.lite.TFLiteConverter.from_saved_model(SAVED_MODEL)
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8
        def representative_dataset_gen():
            for i in range(100):
                data, _ = audio_processor.get_data(1, i*1, model_settings,
                                                BACKGROUND_FREQUENCY,
                                                BACKGROUND_VOLUME_RANGE,
                                                TIME_SHIFT_MS,
                                                'testing',
                                                sess)
                flattened_data = np.array(data.flatten(), dtype=np.float32).reshape(1, 1960)
                yield [flattened_data]
        converter.representative_dataset = representative_dataset_gen
        tflite_model = converter.convert()
        tflite_model_size = open(MODEL_TFLITE, "wb").write(tflite_model)
        print(f"Quantized model is {tflite_model_size} bytes")

    run_tflite_inference_testSet(FLOAT_MODEL_TFLITE)
    run_tflite_inference_testSet(MODEL_TFLITE, model_type='Quantized')
