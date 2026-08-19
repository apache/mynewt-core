#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

from pydantic import BaseModel, ConfigDict, HttpUrl, ValidationError, model_validator
from pathlib import Path
from typing import Any
import yaml
import sys

REPO_ROOT = Path(__file__).resolve().parents[2]
BSP_PATH = REPO_ROOT / "hw" / "bsp"

class BSP(BaseModel):
    model_config = ConfigDict(alias_generator=lambda field: f"bsp.{field}")

    # Core
    flash_map: dict[str, Any]
    compiler: str
    arch: str

    # HW required
    linkerscript: str | list[str] | None = None
    downloadscript: str | None = None
    maker: str | None = None
    name: str | None = None

    # Optional
    optionalcheckscript: str | None = None
    part2linkerscript: str | None = None
    exclude_site: bool | None = False
    image_offset: int | None = None
    debugscript: str | None = None
    image_pad: int | None = None
    url: HttpUrl | None = None

    @model_validator(mode="after")
    def validate_bsp(self) -> BSP:
        errors = []
        is_sim = self.arch.startswith("sim")

        if not is_sim:
            if self.name is None:
                errors.append("Field bsp.name is mandatory for hardware BSP")
            if self.maker is None:
                errors.append("Field bsp.maker is mandatory for hardware BSP")
            # bsp/dialog_cmac is exception
            if self.downloadscript is None and "cortex_m0_cmac" not in self.arch:
                errors.append("Field bsp.downloadscript is mandatory for hardware BSP")
            if self.linkerscript is None:
                errors.append("Field bsp.linkerscript is mandatory for hardware BSP")

        if not self.exclude_site and self.url is None:
            errors.append(
                "Field bsp.url is mandatory when bsp.exclude_site is false or not provided"
            )

        if errors:
            raise ValueError("\n".join(errors))
        return self

def check_bsp_file(board_dir: Path) -> bool:
    bsp_file = board_dir / "bsp.yml"
    dir_name_str = "/".join(bsp_file.parts[-2:])
    try:
        with open(bsp_file, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
            BSP.model_validate(data)
            print(f"Validated {dir_name_str} successfully")
            return True

    except FileNotFoundError:
        print(f"File not found: {dir_name_str}")
        return False

    except yaml.YAMLError as exc:
        print(f"Invalid YAML syntax in {dir_name_str}:\n{exc}")
        return False

    except ValidationError as exc:
        print(f"Schema validation failed for {dir_name_str}:")

        for err in exc.errors():
            clean_msg = err['msg'].replace("Value error, ", "", 1)
            for line in clean_msg.split('\n'):
                print(f"    - {line}")
        return False

if __name__ == "__main__":

    bsp_dir = [d for d in BSP_PATH.iterdir() if d.is_dir()]

    has_errors = False
    for board_dir in sorted(bsp_dir):
        is_valid = check_bsp_file(board_dir)
        if not is_valid:
            has_errors = True

    if has_errors:
        sys.exit(1)
    else:
        sys.exit(0)
