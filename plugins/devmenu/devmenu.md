# Devmenu

Add `DevicePlugin`, `GraphicsPlugin`, then `DevmenuPlugin`. Link the `devmenu`
CMake target. The plugin renders its UI through the public graphics components.

The gamepad Start/Menu button toggles **Dev Menu**. D-pad Up/Down navigate.
The bottom face button selects: it opens folders, toggles booleans, fires buttons
once on press, or begins/commits numeric editing. While editing, D-pad Up/Down
change the value live; the right face button cancels and restores the original
value. Otherwise the right face button goes back or closes the root. Closing
the menu, losing focus or disconnecting the gamepad cancels an unfinished edit.
Gameplay input and simulation continue; the example suspends camera input while
the menu is visible.

`DevmenuResource` configures gamepad bindings, button repeat, position, scale and width.
The menu starts hidden. Its Engine, Device and Graphics pages use public APIs.
Graphics follows the active camera and the first sky/light by entity ID.

## Extensions beside plugin setup

An application-owned container supplies additional root items. Create it before
running the app, or in PostStartup when the target scene entities exist:

```cpp
app.insertResource<frost::DevmenuResource>();
app.addPlugins<frost::DevmenuPlugin>();
app.addSystems(frost::EcsStage::PostStartup,
    +[](frost::World& world, frost::ResMutT<frost::DevmenuResource> menu,
        frost::ResT<frost::ActiveCameraResource> activeCamera)
    {
        menu->custom = world.spawn().id();
        const frost::Entity folder = world.spawn(
            frost::DevmenuItemComponent{ "My game", 3 },
            frost::DevmenuFolderComponent{}).setParent(menu->custom).id();

        world.spawn(frost::DevmenuItemComponent{ "Field of view", 0 },
            frost::DevmenuFloatComponent{ 0, 30, 120, 1, 1 },
            frost::devmenuComponent<&frost::CameraComponent::fov>(activeCamera->entity)
        ).setParent(folder);

        world.spawn(frost::DevmenuItemComponent{ "Show paths", 1 },
            frost::DevmenuBoolComponent{}).setParent(folder);
    });
```

The container itself is invisible; its children appear beside Engine, Device and
Graphics. Devmenu never reparents or destroys that container or its descendants.
Applications may replace `custom`, add/remove items and toggle item visibility at
runtime. Folders append `...` automatically. Siblings sort by `order`, then entity
ID. The root folder entities in `DevmenuResource` are available after Startup for
applications that prefer extending the built-in pages.

Each item has `DevmenuItemComponent` and one folder/button/value/readout component.
Unbound value components hold the setting themselves. A normal application
system can query those values. `DevmenuButtonComponent::activated` lasts one frame
and is intended for application Update systems; `pressed` remains true while held.
Readouts are strings; numeric/bool components also support `readOnly`.

Optional bindings resolve fields on each access, so ECS storage movement is safe.
`devmenuComponent<&Type::member>(entity)` binds a component field;
`devmenuResource<&Type::member>()` binds a resource field. Numeric, enum and bool
fields are supported. Targets that no longer exist hide their items. Component
bindings follow the specified entity; use a custom binding or update its target
to follow a replacement entity. An edit ends when its binding target changes.

Custom systems can supply `DevmenuBindingComponent` read/write/availability
functions for computed settings. These callbacks must not make structural ECS
changes. No callbacks are needed for ordinary value components or buttons.

## Shared graphics UI

The same components can draw HUDs, inventory panels and menus without Devmenu:

```cpp
const frost::Entity hud = world.spawn(frost::UiCanvasComponent{}).id();
const frost::Entity panel = world.spawn(
    frost::UiNodeComponent{ .x = 16, .y = 16, .width = 240 },
    frost::UiLayoutComponent{ frost::UiLayoutComponent::Column, 8, 6 },
    frost::UiRectComponent{ 0x303030ff, 0xffffffff, 1 }
).setParent(hud).id();

world.spawn(frost::UiNodeComponent{},
    frost::UiTextComponent{ "Health" }).setParent(panel);
world.spawn(frost::UiNodeComponent{ .width = 224, .height = 16 },
    frost::UiProgressBarComponent{ .value = 0.75f }).setParent(panel);
```

Canvas coordinates are drawable pixels divided by canvas scale, with the origin
at the top left. Canvas layers draw in ascending order. Nodes use parent-relative
positions, fractional anchors/pivots, optional subtree clipping and sibling order.
Zero node dimensions measure text and row/column layout content. Child nodes
without a layout use explicit positions. Hidden nodes hide their descendants.

Colors are `0xRRGGBBAA`. Text owns a fixed-capacity 1024-byte string and uses bgfx's
built-in 8x8 bitmap font data in a graphics-owned atlas, with point sampling.
The initial text API supports ASCII and newlines. Font assets, text shaping,
mouse interaction, images and world-space UI are future additions.

UI draws after the scene passes, even with no active camera. Rendering is batched
until a clip or canvas boundary, or the batch fills. No ImGui or NanoVG runtime
is used.
