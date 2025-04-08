import { TextField } from "@mui/material";
import { FieldValues, Path, UseFormRegister } from "react-hook-form";

const minLength = 4;

export default function UsernameInput<TFieldValues extends FieldValues>({
  register,
  name,
  errorMessage,
  className = "",
}: {
  register: UseFormRegister<TFieldValues>;
  name: Path<TFieldValues>;
  errorMessage?: string;
  className?: string;
}) {
  return (
    <TextField
      variant="standard"
      label="用户名"
      required
      inputProps={{ maxLength: 100, "aria-label": "username" }}
      {...register(name, {
        required: {
          value: true,
          message: "必填信息.",
        },
        minLength: {
          value: minLength,
          message: `用户名至少为 ${minLength} 个字符.`,
        },
      })}
      error={!!errorMessage}
      helperText={errorMessage}
      className={className}
    />
  );
}
