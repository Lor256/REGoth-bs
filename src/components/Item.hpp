#pragma once
#include <BsPrerequisites.h>
#include <Scene/BsComponent.h>
#include "ScriptBackedBy.hpp"

namespace REGoth
{
  class Item;
  using HItem = bs::GameObjectHandle<Item>;

  /**
   * Component for an item laying on the floor. Will also create and handle
   * the visuals.
   */
  class Item : public ScriptBackedBy
  {
  public:
    Item(const bs::HSceneObject& parent, const bs::String& instance, HGameWorld gameWorld);

  protected:

    void onInitialized() override;
    void onDestroyed() override;

  private:

    /**
     * Create a matching visual from the script object
     */
    void createVisual();

    /**
     * Script object backing this item
     */
    Scripting::ScriptObjectHandle mScriptObject;

  public:
    REGOTH_DECLARE_RTTI(Item);

  protected:
    Item() = default; // For RTTI
  };
}  // namespace REGoth
