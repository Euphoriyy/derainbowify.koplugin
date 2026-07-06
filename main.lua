local Blitbuffer = require("ffi/blitbuffer")
local CreDocument = require("document/credocument")
local CreOptions = require("ui/data/creoptions")
local DataStorage = require("datastorage")
local Device = require("device")
local Dispatcher = require("dispatcher")
local Document = require("document/document")
local KoptInterface = require("document/koptinterface")
local KoptOptions = require("ui/data/koptoptions")
local ReaderConfig = require("apps/reader/modules/readerconfig")
local Screen = Device.screen
local UIManager = require("ui/uimanager")
local WidgetContainer = require("ui/widget/container/widgetcontainer")
local ffi = require("ffi")
local ffiutil = require("ffi/util")
local lfs = require("libs/libkoreader-lfs")
local logger = require("logger")
local optionsutil = require("ui/data/optionsutil")
local util = require("util")
local _ = require("gettext")

-- The level of tolerance for detecting whether a page is colored
local COLOR_TOLERANCE = 20
-- The strength of the filter used to remove the moiré patterns
local FILTER_STRENGTH = 0.9

local Derainbowify = WidgetContainer:extend({
    name = "derainbowify",
    title = _("Derainbowify"),
    is_doc_only = true,
})

-- Disable functionality if a color screen is not detected
if not Device:hasColorScreen() then
    return Derainbowify
end

-----------------------------------------------------

local android = nil

-- Try to detect Kindle running hardfp firmware (from kindle/device.lua)
local function isHardFP()
    return util.pathExists("/lib/ld-linux-armhf.so.3")
end

-- Load FFI libraries
local function get_platform()
    if Device:isDesktop() or Device:isEmulator() then
        return "amd64"
    elseif Device:isKobo() then
        return "kobo"
    elseif Device:isPocketBook() then
        return "pocketbook"
    elseif Device:isKindle() then
        if isHardFP() then
            return "kindlehf"
        end
        return "kindle"
    elseif Device:isAndroid() and util.stringStartsWith(jit.arch, "arm") then
        android = require("android")
        return "android-" .. jit.arch
    end
    return nil
end

local platform = get_platform()
if not platform then
    logger.warn("this platform is not supported by derainbowify")
    return
end

local function fileEquals(source_path, target_path)
    local src_attr = lfs.attributes(source_path)
    local dst_attr = lfs.attributes(target_path)
    if not src_attr or not dst_attr then
        return false
    end
    if dst_attr.size ~= src_attr.size or dst_attr.modification ~= src_attr.modification then
        return false
    end

    local src_hash = util.partialMD5(source_path)
    local dst_hash = util.partialMD5(target_path)
    if not src_hash or not dst_hash then
        return false
    end
    return src_hash == dst_hash
end

local function stage_android_library(source_path)
    if not android or not util.fileExists(source_path) then
        return nil
    end

    local _, filename = util.splitFilePathName(source_path)
    local target_dir = android.dir .. "/plugins/derainbowify.koplugin/libs"
    local target_path = target_dir .. "/" .. filename

    local ok, err = util.makePath(target_dir)
    if not ok then
        logger.warn("Derainbowify: Failed to create staging directory", err)
        return nil
    end

    if not util.fileExists(target_path) or not fileEquals(source_path, target_path) then
        local copy_err = ffiutil.copyFile(source_path, target_path)
        if copy_err then
            logger.warn("Derainbowify: Failed to stage Android library", copy_err)
            return nil
        end
    end

    return target_path
end

local PLUGIN_DIR = DataStorage:getDataDir() .. "/plugins/derainbowify.koplugin"

local color_detect_ok, color_detect, moire_ok, moire
local color_detect_path = PLUGIN_DIR .. string.format("/libs/color_detect-%s.so", platform)
local moire_filter_path = PLUGIN_DIR .. string.format("/libs/moire_filter-%s.so", platform)

if android then
    color_detect_ok, color_detect = pcall(ffi.load, stage_android_library(color_detect_path))
    moire_ok, moire = pcall(ffi.load, stage_android_library(moire_filter_path))
else
    color_detect_ok, color_detect = pcall(ffi.load, color_detect_path)
    moire_ok, moire = pcall(ffi.load, moire_filter_path)
end

if not color_detect_ok then
    logger.warn(string.format("failed to load color_detect-%s.so", platform))
    logger.info(color_detect)
end

if not moire_ok then
    logger.warn(string.format("failed to load moire_filter-%s.so", platform))
    logger.info(moire)
end

if not (color_detect_ok and moire_ok) then
    return
end

ffi.cdef [[
bool is_page_colored(uint8_t* data,
                            int width,
                            int height,
                            int stride,
                            int tolerance);

void remove_moire(unsigned char *fb_data,
                  int width,
                  int height,
                  int stride,
                  bool is_colored,
                  float strength);

int init_moire_resources();

void cleanup_moire_resources();
]]

local moire_initialized = false

function Derainbowify:init()
    if moire_initialized then
        return
    end

    local ok, err = pcall(function()
        moire.init_moire_resources()
    end)

    if ok then
        logger.info("Derainbowify: Initialized moiré resources")
        moire_initialized = true
    else
        logger.warn("Derainbowify: Failed to initialize moiré resources")
        logger.info(err)
    end
end

local original_UIManager_quit = UIManager.quit
function UIManager:quit(...)
    if moire_initialized then
        local ok, err = pcall(function()
            moire.cleanup_moire_resources()
        end)

        if ok then
            logger.info("Derainbowify: Cleaned up moiré resources")
            moire_initialized = false
        else
            logger.warn("Derainbowify: Failed to clean up moiré resources")
            logger.info(err)
        end
    end

    return original_UIManager_quit(self, ...)
end

-----------------------------------------------------

for i, section in ipairs(KoptOptions) do
    if section.icon == "appbar.contrast" then
        table.insert(section.options, 3, {
            name = "derainbow",
            name_text = _("Derainbow"),
            toggle = { _("off"), _("on") },
            values = { 0, 1 },
            default_value = 0,
            show_func = function() return Screen.eink end,
            name_text_hold_callback = optionsutil.showValues,
            help_text = _([[Remove rainbow artifacts from the color e-ink display.]]),
        })
        break
    end
end

for i, section in ipairs(CreOptions) do
    if section.icon == "appbar.contrast" then
        table.insert(section.options, {
            name = "derainbow",
            name_text = _("Derainbow"),
            toggle = { _("off"), _("on") },
            values = { 0, 1 },
            default_value = 0,
            event = "ToggleDerainbow",
            show_func = function() return Screen.eink end,
            name_text_hold_callback = optionsutil.showValues,
            help_text = _([[Remove rainbow artifacts from the color e-ink display.]]),
        })
        break
    end
end

-- Reflowable documents need to be redrawn
function Derainbowify:onToggleDerainbow()
    if self.ui.document.buffer then
        self.ui.document.buffer:free()
        self.ui.document.buffer = nil
    end
    self.ui.rolling:onRedrawCurrentView()
end

-- Load new option
local original_ReaderConfig_init = ReaderConfig.init
function ReaderConfig:init()
    original_ReaderConfig_init(self)

    if self.document.koptinterface ~= nil then
        self.options = KoptOptions
    else
        self.options = CreOptions
    end
    self.configurable:loadDefaults(self.options)
end

-- Register dispatcher actions
Dispatcher:registerAction("kopt_derainbow", {
    category = "configurable",
    paging = true,
})

Dispatcher:registerAction("copt_derainbow", {
    category = "string",
    event = "SetDerainbow",
    title = _("Derainbow"),
    args = { 0, 1 },
    toggle = { _("off"), _("on") },
    rolling = true,
})

function Derainbowify:onSetDerainbow(value)
    if self.ui.document.configurable.derainbow == value then return end
    self.ui.document.configurable.derainbow = value
    self:onToggleDerainbow()
end

-----------------------------------------------------

local function remove_moire_from_bb(bb, copy)
    local filtered_bb = bb
    if copy then
        filtered_bb = Blitbuffer.new(bb.w, bb.h, bb:getType())
        filtered_bb:blitFrom(
            bb,
            0, 0,
            0, 0,
            bb.w,
            bb.h
        )
    end

    local ptr = ffi.cast("uint8_t*", filtered_bb.data)

    -- Check if the buffer has RGB color, >= 24-bit
    local bpp = filtered_bb.stride / filtered_bb.w
    local is_colored = bpp >= 3 and color_detect.is_page_colored(
        ptr,
        filtered_bb.w,
        filtered_bb.h,
        filtered_bb.stride,
        COLOR_TOLERANCE
    )

    local t0 = os.clock()

    moire.remove_moire(
        ptr,
        filtered_bb.w,
        filtered_bb.h,
        filtered_bb.stride,
        is_colored,
        FILTER_STRENGTH
    )

    local t1 = os.clock()
    logger.dbg(string.format("remove_moire: %d x %d = %.1f ms",
        filtered_bb.w, filtered_bb.h, (t1 - t0) * 1000))

    return filtered_bb
end

local function apply_derainbow_to_tile(tile)
    if not tile or tile.derainbow_checked then
        return
    end
    tile.derainbow_checked = true

    tile.derainbow_bb = remove_moire_from_bb(tile.bb, true)
    tile.size = tile.size + tonumber(tile.bb.stride) * tile.bb.h

    -- Set handler for freeing derainbow blitbuffer
    local original_onFree = tile.onFree
    tile.onFree = function(_self)
        original_onFree(_self)
        if _self.derainbow_bb then
            logger.dbg("TileCacheItem: free derainbow blitbuffer", _self.derainbow_bb)
            _self.derainbow_bb:free()
        end
    end
end

local original_KoptInterface_renderPage = KoptInterface.renderPage
function KoptInterface:renderPage(doc, ...)
    local tile = original_KoptInterface_renderPage(self, doc, ...)

    if tile and doc.configurable.derainbow == 1 then
        apply_derainbow_to_tile(tile)

        if tile.derainbow_bb then
            local output_tile = {}
            for k, v in pairs(tile) do
                output_tile[k] = v
            end
            output_tile.bb = tile.derainbow_bb
            return output_tile
        end
    end

    return tile
end

-- Apply the filter to pre-rendered pages
local original_Document_hintPage = Document.hintPage
function Document:hintPage(pageno, zoom, rotation, gamma, saturation, ...)
    if self.configurable.derainbow ~= 1 then
        original_Document_hintPage(self, pageno, zoom, rotation, gamma, saturation, ...)
        return
    end

    logger.dbg("hinting page", pageno)

    local tile = self:renderPage(pageno, nil, zoom, rotation, gamma, saturation, true)
    if tile then
        apply_derainbow_to_tile(tile)
    end
end

-- Apply it for pre-rendered optimized pages (dewatermark/straightened)
local original_KoptInterface_renderOptimizedPage = KoptInterface.renderOptimizedPage
function KoptInterface:renderOptimizedPage(doc, pageno, rect, zoom, rotation, hinting)
    local tile = original_KoptInterface_renderOptimizedPage(self, doc, pageno, rect, zoom, rotation, hinting)

    -- When hinting is disabled, tiles are already filtered in renderPage()
    if tile and doc.configurable.derainbow == 1 and hinting then
        apply_derainbow_to_tile(tile)
    end

    return tile
end

function CreDocument:drawCurrentView(target, x, y, rect, pos)
    if self.buffer and (self.buffer.w ~= rect.w or self.buffer.h ~= rect.h) then
        self.buffer:free()
        self.buffer = nil
    end
    if not self.buffer then
        self.buffer = Blitbuffer.new(rect.w, rect.h, self.render_color and Blitbuffer.TYPE_BBRGB32 or nil)
    end

    self._drawn_images_count, self._drawn_images_surface_ratio =
        self._document:drawCurrentPage(self.buffer, self.render_color, Screen.night_mode and self._nightmode_images,
            self._smooth_scaling, Screen.sw_dithering)

    if self.configurable.derainbow == 1 then
        remove_moire_from_bb(self.buffer, false)
    end
    target:blitFrom(self.buffer, x, y, 0, 0, rect.w, rect.h)
end

-----------------------------------------------------

logger.info("Derainbowify loaded...")

return Derainbowify
