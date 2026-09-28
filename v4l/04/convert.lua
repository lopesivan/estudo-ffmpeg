local input_file = "output_video.raw"
local output_file = "output_video.mp4"

local function convert_video(input, output)
    local command = string.format("ffmpeg -f rawvideo -pix_fmt yuyv422 -s 640x480 -r 30 -i %s -c:v libx264 -preset fast -crf 23 %s", input, output)
    local result = os.execute(command)

    if result then
        print("Video converted successfully.")
    else
        print("An error occurred during video conversion.")
    end
end

convert_video(input_file, output_file)

