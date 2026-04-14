"""AskUserQuestion implementation."""


def ask_user(questions: list) -> str:
    """Present multiple-choice questions to the user and collect answers."""
    answers = {}
    print()
    for q in questions:
        print(f"  Q: {q['question']}")
        opts = q.get("options", [])
        for i, opt in enumerate(opts):
            desc = f" — {opt['description']}" if opt.get("description") else ""
            print(f"    {i + 1}. {opt['label']}{desc}")
        print(f"    {len(opts) + 1}. Other (free input)")

        choice = input("  Choice: ").strip()
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(opts):
                answers[q["question"]] = opts[idx]["label"]
            else:
                answers[q["question"]] = input("  Input: ").strip()
        except ValueError:
            answers[q["question"]] = choice

    qa = ", ".join(f'"{k}"="{v}"' for k, v in answers.items())
    return f"User has answered your questions: {qa}. You can now continue with the user's answers in mind."
