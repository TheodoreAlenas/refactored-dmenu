typedef void (*FunctionToRepeat) (int text_width, void *extra_argumens);
typedef void (*RepeatForAllItemWidths) (FunctionToRepeat, void *extra_arguments);
int get_typical_width(int typicality, RepeatForAllItemWidths repeat_for_all_item_widths);

