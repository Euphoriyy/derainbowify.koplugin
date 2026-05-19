local Blitbuffer = require("ffi/blitbuffer")
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
local logger = require("logger")
local optionsutil = require("ui/data/optionsutil")
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

-- Load FFI libraries
local function get_platform()
    if Device:isDesktop() or Device:isEmulator() then
        return "amd64"
    elseif Device:isKobo() then
        return "kobo"
    elseif Device:isPocketbook() then
        return "pocketbook"
    elseif Device:isKindle() then
        if Device:isHardFP() then
            return "kindlehf"
        end
        return "kindle"
    end
    return nil
end

local platform = get_platform()
if not platform then
    logger.warn("this platform is not supported by derainbowify")
    return
end

local PLUGIN_DIR = DataStorage:getDataDir() .. "/plugins/derainbowify.koplugin"

local color_detect_ok, color_detect = pcall(ffi.load, PLUGIN_DIR .. string.format("/libs/color_detect-%s.so", platform))
local moire_ok, moire = pcall(ffi.load, PLUGIN_DIR .. string.format("/libs/moire_filter-%s.so", platform))

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
                  float strength);

int init_moire_resources();

void cleanup_moire_resources();
]]

function Derainbowify:init()
    pcall(function()
        moire.init_moire_resources()
    end)
end

local original_UIManager_quit = UIManager.quit
function UIManager:quit(...)
    pcall(function()
        moire.cleanup_moire_resources()
    end)

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

-- Load new option
local original_ReaderConfig_init = ReaderConfig.init
function ReaderConfig:init()
    original_ReaderConfig_init(self)

    if self.document.koptinterface ~= nil then
        self.options = KoptOptions
        self.configurable:loadDefaults(self.options)
    end
end

-- Register dispatcher action
Dispatcher:registerAction("kopt_derainbow", {
    category = "configurable",
    paging = true,
})

-----------------------------------------------------

local function remove_moire_from_tile(tile)
    local filtered_bb = Blitbuffer.new(
        tile.bb.w,
        tile.bb.h,
        tile.bb:getType()
    )
    filtered_bb:blitFrom(
        tile.bb,
        0, 0,
        0, 0,
        tile.bb.w,
        tile.bb.h
    )

    local ptr = ffi.cast("uint8_t*", filtered_bb.data)

    local t0 = os.clock()

    moire.remove_moire(
        ptr,
        filtered_bb.w,
        filtered_bb.h,
        filtered_bb.stride,
        FILTER_STRENGTH
    )

    local t1 = os.clock()
    logger.dbg(string.format("remove_moire: %d x %d = %.1f ms",
        filtered_bb.w, filtered_bb.h, (t1 - t0) * 1000))

    return filtered_bb
end

local function apply_derainbow_to_tile(tile)
    if not tile or tile.derainbow_checked then return end
    tile.derainbow_checked = true

    local ptr = ffi.cast("uint8_t*", tile.bb.data)
    local is_colored = color_detect.is_page_colored(
        ptr,
        tile.bb.w,
        tile.bb.h,
        tile.bb.stride,
        COLOR_TOLERANCE
    )

    if not is_colored then
        tile.derainbow_bb = remove_moire_from_tile(tile)
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
end

local original_KoptInterface_renderPage = KoptInterface.renderPage
function KoptInterface:renderPage(doc, ...)
    local tile = original_KoptInterface_renderPage(self, doc, ...)

    if doc.configurable.derainbow == 1 then
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
function Document:hintPage(pageno, zoom, rotation, gamma)
    if self.configurable.derainbow ~= 1 then
        original_Document_hintPage(self, pageno, zoom, rotation, gamma)
        return
    end

    logger.dbg("hinting page", pageno)

    local tile = self:renderPage(pageno, nil, zoom, rotation, gamma, true)
    apply_derainbow_to_tile(tile)
end

-- Apply it for pre-rendered optimized pages (dewatermark/straightened)
local original_KoptInterface_renderOptimizedPage = KoptInterface.renderOptimizedPage
function KoptInterface:renderOptimizedPage(doc, pageno, rect, zoom, rotation, gamma, hinting)
    local tile = original_KoptInterface_renderOptimizedPage(self, doc, pageno, rect, zoom, rotation, gamma, hinting)

    -- When hinting is disabled, tiles are already filtered in renderPage()
    if doc.configurable.derainbow == 1 and hinting then
        apply_derainbow_to_tile(tile)
    end

    return tile
end

-----------------------------------------------------

logger.info("Derainbowify loaded...")

return Derainbowify
