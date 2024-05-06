/*
 *
 * Copyright (C) 2022 Xilinx, Inc.
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vvas_efficientdetd2.hpp"
#include <algorithm>

vvas_efficientdetd2::vvas_efficientdetd2 (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::EfficientDetD2::create (model_name, need_preprocess);
}

bool
compare_by_area (const vitis::ai::EfficientDetD2Result::BoundingBox & box1,
    const vitis::ai::EfficientDetD2Result::BoundingBox & box2)
{
  float area1 = (box1.width * box1.height);
  float area2 = (box2.width * box2.height);
  return (area1 > area2);
}

int
vvas_efficientdetd2::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter batch");
  auto results = model->run (images);

  char *pstr;                   /* prediction string */

  for (auto i = 0u; i < results.size (); i++) {
    VvasInferPrediction *parent_predict = NULL;
    unsigned int cur_objs = 0;

    LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "objects detected %lu",
        results[i].bboxes.size ());

    if (results[i].bboxes.size ()) {
      VvasBoundingBox parent_bbox = { 0 };
      int cols = images[i].cols;
      int rows = images[i].rows;

      parent_predict = predictions[i];

    for (auto & box:results[i].bboxes) {

        if (!parent_predict) {
          parent_bbox.x = parent_bbox.y = 0;
          parent_bbox.width = cols;
          parent_bbox.height = rows;
          parent_predict = vvas_inferprediction_new ();
          parent_predict->bbox = parent_bbox;
        }
        int label = box.label;
        float xmin = box.x * cols + 1;
        float ymin = box.y * rows + 1;
        float xmax = xmin + box.width * cols;
        float ymax = ymin + box.height * rows;
        if (xmin < 0.)
          xmin = 1.;
        if (ymin < 0.)
          ymin = 1.;
        if (xmax > cols)
          xmax = cols;
        if (ymax > rows)
          ymax = rows;
        float confidence = box.score;

        VvasBoundingBox bbox = { 0 };
        VvasInferPrediction *predict;
        VvasInferClassification *c = NULL;

        bbox.x = xmin;
        bbox.y = ymin;
        bbox.width = xmax - xmin;
        bbox.height = ymax - ymin;

        predict = vvas_inferprediction_new ();
        predict->bbox = bbox;

        c = vvas_inferclassification_new ();
        c->class_id = label;
        c->class_prob = confidence;
        c->num_classes = 0;
        predict->classifications = vvas_list_append (predict->classifications, c);

        if (parent_predict->node == NULL)
          LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
              "parent_predict->predictions is NULL");

	/* add class and name in prediction node */
        predict->model_class = (VvasClass) kpriv->modelclass;
        predict->model_name = strdup (kpriv->modelname.c_str ());
        vvas_inferprediction_append (parent_predict, predict);

        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level,
            "RESULT: (%d) %f %f %f %f (%f)", label,
            xmin, ymin, xmax, ymax, confidence);

        cur_objs++;
        if (cur_objs == kpriv->objs_detection_max) {
          LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
              "reached max limit of objects to add to metadata");
          break;
        }
      }

      if (parent_predict && kpriv->log_level >= LOG_LEVEL_DEBUG) {
        pstr = vvas_inferprediction_to_string (parent_predict);
        LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
            "prediction tree : \n%s", pstr);
        free (pstr);
      }
    }
    predictions[i] = parent_predict;
  }

  LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, " ");

  return true;
}

int
vvas_efficientdetd2::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_efficientdetd2::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_efficientdetd2::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_efficientdetd2::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_efficientdetd2::~vvas_efficientdetd2 ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
