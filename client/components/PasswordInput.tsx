import { Visibility, VisibilityOff } from "@mui/icons-material";
import { IconButton, TextField } from "@mui/material";
import { useReducer } from "react";
import { FieldValues, Path, UseFormRegister } from "react-hook-form";

const reducer = (prev: boolean) => !prev;

const minLength = 10;

export default function PasswordInput<TFieldValues extends FieldValues>({
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
  const [showPassword, triggerShowPassword] = useReducer(reducer, false);
  return (
    <TextField
      variant="standard"
      label="密码"
      required
      inputProps={{ maxLength: 100, "aria-label": "password" }}
      InputProps={{
        endAdornment: (
          <IconButton onClick={triggerShowPassword} role="button">
            {showPassword ? <VisibilityOff /> : <Visibility />}
          </IconButton>
        ),
      }}
      type={showPassword ? "text" : "password"}
      {...register(name, {
        required: {
          value: true,
          message: "请填写信息.",
        },
        minLength: {
          value: minLength,
          message: `至少输入 ${minLength} 个字符.`,
        },
      })}
      error={!!errorMessage}
      helperText={errorMessage}
      className={className}
    />
  );
}
