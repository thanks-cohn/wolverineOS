#!/usr/bin/env python3
import json
import subprocess
import sys
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QFileDialog, QListWidget, QListWidgetItem, QTextEdit, QLabel,
    QMessageBox, QInputDialog, QLineEdit, QComboBox
)


IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".gif"}


class ImetaGuiV2(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("imeta Control Panel v2")
        self.resize(1250, 780)

        self.current_dir = None
        self.current_file = None
        self.current_sidecar = None
        self.memory = {}

        root = QVBoxLayout()
        self.status = QLabel("Choose a folder to begin.")
        root.addWidget(self.status)

        top = QHBoxLayout()

        self.btn_choose = QPushButton("Choose Folder")
        self.btn_choose.clicked.connect(self.choose_folder)
        top.addWidget(self.btn_choose)

        self.btn_start = QPushButton("Start Watcher")
        self.btn_start.clicked.connect(lambda: self.systemctl("start"))
        top.addWidget(self.btn_start)

        self.btn_stop = QPushButton("Stop Watcher")
        self.btn_stop.clicked.connect(lambda: self.systemctl("stop"))
        top.addWidget(self.btn_stop)

        self.btn_enable = QPushButton("Start Automatically on Login")
        self.btn_enable.clicked.connect(lambda: self.systemctl("enable"))
        top.addWidget(self.btn_enable)

        self.btn_disable = QPushButton("Disable Automatic Start")
        self.btn_disable.clicked.connect(lambda: self.systemctl("disable"))
        top.addWidget(self.btn_disable)

        root.addLayout(top)

        actions = QHBoxLayout()

        self.btn_bind = QPushButton("Bind Selected")
        self.btn_bind.clicked.connect(self.bind_selected)
        actions.addWidget(self.btn_bind)

        self.btn_bind_folder = QPushButton("Bind Whole Folder")
        self.btn_bind_folder.clicked.connect(self.bind_folder)
        actions.addWidget(self.btn_bind_folder)

        self.btn_scan = QPushButton("Scan")
        self.btn_scan.clicked.connect(lambda: self.run_imeta(["scan", str(self.current_dir), "--verbose"]))
        actions.addWidget(self.btn_scan)

        self.btn_doctor = QPushButton("Doctor")
        self.btn_doctor.clicked.connect(lambda: self.run_imeta(["doctor", str(self.current_dir), "--verbose"]))
        actions.addWidget(self.btn_doctor)

        self.btn_reconcile = QPushButton("Reconcile")
        self.btn_reconcile.clicked.connect(lambda: self.run_imeta(["reconcile", str(self.current_dir), "--verbose"]))
        actions.addWidget(self.btn_reconcile)

        self.btn_refresh_memory = QPushButton("Refresh Memory")
        self.btn_refresh_memory.clicked.connect(self.load_memory)
        actions.addWidget(self.btn_refresh_memory)

        root.addLayout(actions)

        main = QHBoxLayout()

        left = QVBoxLayout()
        left.addWidget(QLabel("Files, multi-select enabled"))
        self.files = QListWidget()
        self.files.setSelectionMode(QListWidget.ExtendedSelection)
        self.files.itemClicked.connect(self.load_selected_file)
        left.addWidget(self.files)
        main.addLayout(left, 2)

        center = QVBoxLayout()
        center.addWidget(QLabel("Preview"))
        self.preview = QLabel("No image selected.")
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setMinimumSize(420, 420)
        self.preview.setStyleSheet("border: 1px solid #555; background: #111;")
        center.addWidget(self.preview)

        self.preview_name = QLabel("")
        self.preview_name.setAlignment(Qt.AlignCenter)
        center.addWidget(self.preview_name)
        main.addLayout(center, 3)

        right = QVBoxLayout()

        right.addWidget(QLabel("Bulk Tags"))
        self.tags_input = QLineEdit()
        self.tags_input.setPlaceholderText("red, blue, night, rain")
        right.addWidget(self.tags_input)

        self.memory_combo = QComboBox()
        self.memory_combo.setEditable(False)
        self.memory_combo.currentTextChanged.connect(self.memory_value_chosen)
        right.addWidget(self.memory_combo)

        tag_buttons = QHBoxLayout()

        self.btn_apply_tags = QPushButton("Append Tags to Selected")
        self.btn_apply_tags.clicked.connect(self.bulk_append_tags)
        tag_buttons.addWidget(self.btn_apply_tags)

        self.btn_apply_field = QPushButton("Append Field to Selected")
        self.btn_apply_field.clicked.connect(self.bulk_append_field)
        tag_buttons.addWidget(self.btn_apply_field)

        right.addLayout(tag_buttons)

        right.addWidget(QLabel(".imeta JSON"))
        self.editor = QTextEdit()
        right.addWidget(self.editor, 3)

        json_buttons = QHBoxLayout()

        self.btn_add_field = QPushButton("+ Add Field")
        self.btn_add_field.clicked.connect(self.add_field)
        json_buttons.addWidget(self.btn_add_field)

        self.btn_remove_field = QPushButton("- Remove Field")
        self.btn_remove_field.clicked.connect(self.remove_field)
        json_buttons.addWidget(self.btn_remove_field)

        self.btn_save = QPushButton("Save .imeta")
        self.btn_save.clicked.connect(self.save_sidecar)
        json_buttons.addWidget(self.btn_save)

        right.addLayout(json_buttons)

        right.addWidget(QLabel("Logs"))
        self.logs = QTextEdit()
        self.logs.setReadOnly(True)
        right.addWidget(self.logs, 2)

        main.addLayout(right, 4)
        root.addLayout(main)

        self.setLayout(root)
        self.load_memory()

    def log(self, text):
        if text:
            self.logs.append(str(text))

    def run_cmd(self, cmd):
        self.log("$ " + " ".join(cmd))
        try:
            result = subprocess.run(
                cmd,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.log(result.stdout.strip())
            return result.returncode
        except Exception as e:
            self.log(f"[GUI ERROR] {e}")
            return 1

    def run_imeta(self, args):
        if self.current_dir is None:
            QMessageBox.warning(self, "No folder", "Choose a folder first.")
            return 1
        return self.run_cmd(["imeta"] + args)

    def systemctl(self, action):
        self.run_cmd(["systemctl", "--user", action, "imeta-watchd"])

    def choose_folder(self):
        folder = QFileDialog.getExistingDirectory(self, "Choose folder")
        if not folder:
            return
        self.current_dir = Path(folder)
        self.status.setText(f"Current folder: {self.current_dir}")
        self.refresh_file_list()
        self.load_memory()

    def refresh_file_list(self):
        self.files.clear()
        if not self.current_dir:
            return

        for path in sorted(self.current_dir.iterdir()):
            if path.is_file() and not path.name.endswith(".imeta"):
                item = QListWidgetItem(path.name)
                item.setData(Qt.UserRole, str(path))
                self.files.addItem(item)

    def selected_paths(self):
        paths = []
        for item in self.files.selectedItems():
            raw = item.data(Qt.UserRole)
            if raw:
                paths.append(Path(raw))
        return paths

    def load_selected_file(self, item):
        self.current_file = Path(item.data(Qt.UserRole))
        self.current_sidecar = Path(str(self.current_file) + ".imeta")
        self.preview_file(self.current_file)

        if not self.current_sidecar.exists():
            self.editor.setText(json.dumps(self.default_sidecar(self.current_file), indent=2))
            self.status.setText(f"Loaded new sidecar draft for: {self.current_file.name}")
            return

        try:
            data = json.loads(self.current_sidecar.read_text())
            self.editor.setText(json.dumps(data, indent=2))
            self.status.setText(f"Loaded: {self.current_sidecar}")
        except Exception as e:
            QMessageBox.warning(self, "JSON error", f"Could not read sidecar:\n{e}")

    def preview_file(self, path):
        self.preview_name.setText(path.name)

        if path.suffix.lower() not in IMAGE_EXTS:
            self.preview.setText("Preview available for images only.")
            self.preview.setPixmap(QPixmap())
            return

        pix = QPixmap(str(path))
        if pix.isNull():
            self.preview.setText("Could not load image preview.")
            return

        scaled = pix.scaled(
            self.preview.width(),
            self.preview.height(),
            Qt.KeepAspectRatio,
            Qt.SmoothTransformation
        )
        self.preview.setPixmap(scaled)

    def default_sidecar(self, file_path):
        sidecar = Path(str(file_path) + ".imeta")
        return {
            "imeta_version": "0.1.0",
            "kind": "sidecar",
            "asset": {
                "path": str(file_path),
                "sidecar_path": str(sidecar),
            },
            "identity": {},
            "annotations": {
                "caption": "",
                "tags": [],
                "tag_meta": {}
            },
            "training": {
                "enabled": False,
                "type": "none"
            },
            "custom": {}
        }

    def load_sidecar_data(self, file_path):
        sidecar = Path(str(file_path) + ".imeta")
        if not sidecar.exists():
            return self.default_sidecar(file_path)

        try:
            return json.loads(sidecar.read_text())
        except Exception:
            return self.default_sidecar(file_path)

    def save_sidecar_data(self, file_path, data):
        sidecar = Path(str(file_path) + ".imeta")
        sidecar.write_text(json.dumps(data, indent=2) + "\n")

    def get_editor_json(self):
        try:
            return json.loads(self.editor.toPlainText())
        except Exception as e:
            QMessageBox.warning(self, "Invalid JSON", f"JSON is not valid:\n{e}")
            return None

    def set_editor_json(self, data):
        self.editor.setText(json.dumps(data, indent=2))

    def parse_value(self, value):
        raw = value.strip()

        if raw == "":
            return ""

        if raw.lower() == "true":
            return True
        if raw.lower() == "false":
            return False
        if raw.lower() == "null":
            return None

        try:
            if "." in raw:
                return float(raw)
            return int(raw)
        except ValueError:
            pass

        if raw.startswith("[") or raw.startswith("{"):
            try:
                return json.loads(raw)
            except Exception:
                return raw

        return raw

    def parse_tags(self, text):
        return [x.strip() for x in text.split(",") if x.strip()]

    def ensure_path(self, data, keys):
        node = data
        for key in keys:
            if key not in node or not isinstance(node[key], dict):
                node[key] = {}
            node = node[key]
        return node

    def append_unique_list(self, data, field_path, values):
        keys = [k.strip() for k in field_path.split(".") if k.strip()]
        if not keys:
            return

        parent = self.ensure_path(data, keys[:-1])
        last = keys[-1]

        if last not in parent or not isinstance(parent[last], list):
            parent[last] = []

        for value in values:
            if value not in parent[last]:
                parent[last].append(value)

    def set_field(self, data, field_path, value):
        keys = [k.strip() for k in field_path.split(".") if k.strip()]
        if not keys:
            return
        parent = self.ensure_path(data, keys[:-1])
        parent[keys[-1]] = value

    def add_field(self):
        data = self.get_editor_json()
        if data is None:
            return

        path, ok = QInputDialog.getText(
            self,
            "Add Field",
            "Field path, example: custom.artist_notes.mood"
        )
        if not ok or not path.strip():
            return

        value, ok = QInputDialog.getText(
            self,
            "Field Value",
            "Value. JSON allowed: true, 3, 0.92, [\"red\",\"blue\"]"
        )
        if not ok:
            return

        parsed = self.parse_value(value)
        self.set_field(data, path, parsed)
        self.set_editor_json(data)
        self.remember(path, value, str(self.current_file) if self.current_file else "")

    def remove_field(self):
        data = self.get_editor_json()
        if data is None:
            return

        path, ok = QInputDialog.getText(
            self,
            "Remove Field",
            "Field path, example: custom.artist_notes.mood"
        )
        if not ok or not path.strip():
            return

        keys = [k.strip() for k in path.split(".") if k.strip()]
        if not keys:
            return

        node = data
        for key in keys[:-1]:
            if key not in node or not isinstance(node[key], dict):
                QMessageBox.warning(self, "Not found", "That field path does not exist.")
                return
            node = node[key]

        if keys[-1] in node:
            del node[keys[-1]]
            self.set_editor_json(data)
        else:
            QMessageBox.warning(self, "Not found", "That field does not exist.")

    def save_sidecar(self):
        if not self.current_file:
            QMessageBox.warning(self, "No file", "Select a file first.")
            return

        data = self.get_editor_json()
        if data is None:
            return

        self.save_sidecar_data(self.current_file, data)
        self.status.setText(f"Saved: {self.current_file}.imeta")

    def bind_selected(self):
        paths = self.selected_paths()
        if not paths and self.current_file:
            paths = [self.current_file]

        if not paths:
            QMessageBox.warning(self, "No file", "Select at least one file.")
            return

        for path in paths:
            self.run_cmd(["imeta", "bind", str(path), "--verbose"])

        self.refresh_file_list()

    def bind_folder(self):
        if not self.current_dir:
            QMessageBox.warning(self, "No folder", "Choose a folder first.")
            return

        self.run_cmd(["imeta", "scan", str(self.current_dir), "--verbose"])
        self.run_cmd(["imeta", "bind-missing", str(self.current_dir), "--verbose"])
        self.run_cmd(["imeta", "scan", str(self.current_dir), "--verbose"])
        self.refresh_file_list()

    def bulk_append_tags(self):
        paths = self.selected_paths()
        tags = self.parse_tags(self.tags_input.text())

        if not paths:
            QMessageBox.warning(self, "No selection", "Select one or more files.")
            return

        if not tags:
            QMessageBox.warning(self, "No tags", "Enter tags separated by commas.")
            return

        for path in paths:
            data = self.load_sidecar_data(path)
            if "annotations" not in data or not isinstance(data["annotations"], dict):
                data["annotations"] = {}

            if "tags" not in data["annotations"] or not isinstance(data["annotations"]["tags"], list):
                data["annotations"]["tags"] = []

            for tag in tags:
                if tag not in data["annotations"]["tags"]:
                    data["annotations"]["tags"].append(tag)
                self.remember("annotations.tags", tag, str(path))

            self.save_sidecar_data(path, data)

        self.log(f"[GUI] appended tags to {len(paths)} file(s): {tags}")

        if self.current_file:
            self.set_editor_json(self.load_sidecar_data(self.current_file))

        self.load_memory()

    def bulk_append_field(self):
        paths = self.selected_paths()

        if not paths:
            QMessageBox.warning(self, "No selection", "Select one or more files.")
            return

        field_path, ok = QInputDialog.getText(
            self,
            "Bulk Append Field",
            "Field path, example: annotations.tag_meta.red.shade"
        )
        if not ok or not field_path.strip():
            return

        value, ok = QInputDialog.getText(
            self,
            "Bulk Field Value",
            "Value. JSON allowed: true, 3, 0.92, [\"red\",\"blue\"]"
        )
        if not ok:
            return

        parsed = self.parse_value(value)

        for path in paths:
            data = self.load_sidecar_data(path)
            self.set_field(data, field_path, parsed)
            self.save_sidecar_data(path, data)
            self.remember(field_path, value, str(path))

        self.log(f"[GUI] applied field to {len(paths)} file(s): {field_path} = {value}")

        if self.current_file:
            self.set_editor_json(self.load_sidecar_data(self.current_file))

        self.load_memory()

    def remember(self, field_path, value, source_file):
        self.run_cmd(["./imeta", "remember", field_path, str(value), source_file])

    def load_memory(self):
        self.memory = {}
        self.memory_combo.clear()
        self.memory_combo.addItem("Memory suggestions...")

        memory_path = Path(".imeta/memory.jsonl")
        if not memory_path.exists():
            return

        seen = set()

        try:
            for line in memory_path.read_text().splitlines():
                if not line.strip():
                    continue
                row = json.loads(line)
                field = row.get("field_path", "")
                value = row.get("value", "")
                if not field or value == "":
                    continue

                key = (field, value)
                if key in seen:
                    continue
                seen.add(key)

                self.memory.setdefault(field, []).append(value)
                self.memory_combo.addItem(f"{field} = {value}")
        except Exception as e:
            self.log(f"[GUI][WARN] failed to load memory: {e}")

    def memory_value_chosen(self, text):
        if not text or text == "Memory suggestions...":
            return

        if " = " not in text:
            return

        field, value = text.split(" = ", 1)
        self.tags_input.setText(value)
        self.log(f"[GUI] memory selected: {field} = {value}")


def main():
    app = QApplication(sys.argv)
    win = ImetaGuiV2()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
