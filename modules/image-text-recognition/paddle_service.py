#!/usr/bin/env python3
"""
PaddleOCR persistent service for lele-tools.
Loads the model once, then accepts OCR requests via stdin (JSON lines)
and returns results via stdout (JSON lines).

Protocol:
  Startup:  stdout <- {"status":"ready"}
  Request:  stdin  -> {"id":"xxx","images":["/path/img1.png",...]}
  Response: stdout <- {"id":"xxx","results":[{"path":"...","texts":[...],"polys":[...],"scores":[...]},...]}
  Shutdown: close stdin (EOF)
"""

import sys
import json
import traceback

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["fast", "accurate"], default="fast",
                        help="fast=mobile model, accurate=server model")
    args = parser.parse_args()

    # Suppress paddle/paddleocr logs
    import os
    os.environ["GLOG_minloglevel"] = "3"
    os.environ["FLAGS_log_level"] = "3"

    try:
        from paddleocr import PaddleOCR
    except ImportError:
        sys.stdout.write(json.dumps({"status": "error", "message": "PaddleOCR not installed"}) + "\n")
        sys.stdout.flush()
        sys.exit(1)

    try:
        kwargs = {
            "use_doc_orientation_classify": False,
            "use_doc_unwarping": False,
            "use_textline_orientation": False,
        }
        if args.mode == "fast":
            kwargs["text_detection_model_name"] = "PP-OCRv5_mobile_det"
            kwargs["text_recognition_model_name"] = "PP-OCRv5_mobile_rec"
        # else: use default server models (higher accuracy but slower)
        ocr = PaddleOCR(**kwargs)
    except Exception as e:
        sys.stdout.write(json.dumps({"status": "error", "message": str(e)}) + "\n")
        sys.stdout.flush()
        sys.exit(1)

    # Signal ready
    sys.stdout.write(json.dumps({"status": "ready"}) + "\n")
    sys.stdout.flush()

    # Request loop
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            sys.stdout.write(json.dumps({"error": f"Invalid JSON: {e}"}) + "\n")
            sys.stdout.flush()
            continue

        req_id = req.get("id", "")
        img_path = req.get("image", "")

        try:
            ocr_result = ocr.predict(img_path)
            texts = []
            polys = []
            scores = []
            for res in ocr_result:
                rec_texts = res.get("rec_texts", [])
                rec_scores = res.get("rec_scores", [])
                dt_polys = res.get("dt_polys", [])

                for i, t in enumerate(rec_texts):
                    t_str = str(t).strip()
                    if not t_str:
                        continue
                    texts.append(t_str)
                    if i < len(rec_scores):
                        s = rec_scores[i]
                        scores.append(float(s) if hasattr(s, '__float__') else 0.0)
                    else:
                        scores.append(0.0)
                    if i < len(dt_polys):
                        poly = dt_polys[i]
                        if hasattr(poly, 'tolist'):
                            poly = poly.tolist()
                        else:
                            poly = [[int(pt[0]), int(pt[1])] for pt in poly]
                        polys.append(poly)
                    else:
                        polys.append([])

            response = {
                "id": req_id,
                "path": img_path,
                "texts": texts,
                "polys": polys,
                "scores": scores
            }
        except Exception as e:
            response = {
                "id": req_id,
                "path": img_path,
                "texts": [],
                "polys": [],
                "scores": [],
                "error": str(e)
            }

        sys.stdout.write(json.dumps(response, ensure_ascii=False) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
