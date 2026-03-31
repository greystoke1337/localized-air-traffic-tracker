import displayio
import time
import terminalio
from adafruit_matrixportal.matrix import Matrix
from adafruit_display_text import bitmap_label

matrix = Matrix(width=64, height=32, bit_depth=2)
display = matrix.display
display.rotation = 180

group = displayio.Group()

label = bitmap_label.Label(
    terminalio.FONT,
    text="HELLO WORLD",
    color=0xFF0000,
    anchor_point=(0, 0.5),
    anchored_position=(64, 16),
)
group.append(label)
display.root_group = group

text_width = label.bounding_box[2]
print("text_width:", text_width)

x = 64
while True:
    label.x = x
    display.refresh()
    x -= 1
    if x < -text_width:
        x = 64
    time.sleep(0.03)
